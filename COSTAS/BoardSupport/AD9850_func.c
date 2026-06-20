#include <stdio.h>
#include <stdint.h>
#include "AD9850_func.h"
#include "NUC1xx.h"
#include "DrvGPIO.h"
#include "DrvSYS.h"
#include "DrvSPI.h"
#include "DrvTIMER.h"

// 板级 AD9850 驱动，负责通过 8 位并行总线配置两片 AD9850 DDS。
// 全局状态变量：w0..w4 是写入 AD9850 的 5 个控制字节，w0 保存相位字节，w1..w4 保存频率字节。
// 临时变量：dds_fq 保存频率调谐字拆分过程中的中间值，dds_ph 保存相位控制字节。
// 更新时间：2026.06 氧化物

#define AD9850_REFERENCE_HZ_DEFAULT 125000000UL
#define AD9850_REFERENCE_HZ_MIN     120000000UL
#define AD9850_REFERENCE_HZ_MAX     130000000UL

// AD9850 控制引脚
#define W_CKL_H    DrvGPIO_SetBit(E_GPC,6)
#define W_CKL_L    DrvGPIO_ClrBit(E_GPC,6)
#define W_CKR_H    DrvGPIO_SetBit(E_GPC,4)
#define W_CKR_L    DrvGPIO_ClrBit(E_GPC,4)
#define FQ_UD_H    DrvGPIO_SetBit(E_GPC,5)
#define FQ_UD_L    DrvGPIO_ClrBit(E_GPC,5)
#define RST_H      DrvGPIO_SetBit(E_GPC,7)
#define RST_L      DrvGPIO_ClrBit(E_GPC,7)

unsigned char w0, w1, w2, w3, w4;
unsigned long dds_fq;
unsigned char dds_ph;
static uint32_t ad9850_reference_hz = AD9850_REFERENCE_HZ_DEFAULT;

// 1 将 32 位频率控制字和 5 位相位字写入右侧 AD9850。
// 2 freq_word: AD9850 32 位频率控制字；phase: 0..31 相位控制字，每步 11.25 度。
static void AD9850_WriteRight(uint32_t freq_word, unsigned char phase);
// 1 将 32 位频率控制字和 5 位相位字写入左侧 AD9850。
// 2 freq_word: AD9850 32 位频率控制字；phase: 0..31 相位控制字，每步 11.25 度。
static void AD9850_WriteLeft(uint32_t freq_word, unsigned char phase);



void ad9850_Port_Init(void) {
    DrvGPIO_Open(E_GPC, 6, E_IO_OUTPUT); // W_CKL
    DrvGPIO_Open(E_GPC, 4, E_IO_OUTPUT); // W_CKR
    DrvGPIO_Open(E_GPC, 5, E_IO_OUTPUT); // FQ_UD
    DrvGPIO_Open(E_GPC, 7, E_IO_OUTPUT); // RST
    DrvGPIO_Open(E_GPE, 5, E_IO_OUTPUT); // D0
    DrvGPIO_Open(E_GPE, 3, E_IO_OUTPUT); // D1
    DrvGPIO_Open(E_GPE, 1, E_IO_OUTPUT); // D2
    DrvGPIO_Open(E_GPE, 8, E_IO_OUTPUT); // D3
    DrvGPIO_Open(E_GPE, 6, E_IO_OUTPUT); // D4
    DrvGPIO_Open(E_GPE, 4, E_IO_OUTPUT); // D5
    DrvGPIO_Open(E_GPE, 2, E_IO_OUTPUT); // D6
    DrvGPIO_Open(E_GPE, 0, E_IO_OUTPUT); // D7
}
void ad9850_reset(void) {
    FQ_UD_L;
    W_CKL_L;
    W_CKR_L;

    RST_L;
    RST_H;
    RST_H;
    RST_H; // 保持High三回，用来确保RST生效
    RST_L;
}
void Clock_Pluse(void) {
    W_CKL_L;

    W_CKL_H;
    W_CKL_H;
    RST_H;
    W_CKL_H;
    W_CKL_H;

    W_CKL_L;
    RST_L;
}


void ad9850_wr_parrel(unsigned char w) {
    if ((w & 0x01) == 0) // D0
        DrvGPIO_ClrBit(E_GPE, 5);
    else
        DrvGPIO_SetBit(E_GPE, 5);

    if ((w & 0x02) == 0) // D1
        DrvGPIO_ClrBit(E_GPE, 3);
    else
        DrvGPIO_SetBit(E_GPE, 3);

    if ((w & 0x04) == 0) // D2
        DrvGPIO_ClrBit(E_GPE, 1);
    else
        DrvGPIO_SetBit(E_GPE, 1);

    if ((w & 0x08) == 0) // D3
        DrvGPIO_ClrBit(E_GPE, 8);
    else
        DrvGPIO_SetBit(E_GPE, 8);

    if ((w & 0x10) == 0) // D4
        DrvGPIO_ClrBit(E_GPE, 6);
    else
        DrvGPIO_SetBit(E_GPE, 6);

    if ((w & 0x20) == 0) // D5
        DrvGPIO_ClrBit(E_GPE, 4);
    else
        DrvGPIO_SetBit(E_GPE, 4);

    if ((w & 0x40) == 0) // D6
        DrvGPIO_ClrBit(E_GPE, 2);
    else
        DrvGPIO_SetBit(E_GPE, 2);

    if ((w & 0x80) == 0) // D7
        DrvGPIO_ClrBit(E_GPE, 0);
    else
        DrvGPIO_SetBit(E_GPE, 0);
}


void AD9850_SetReferenceHz(uint32_t reference_hz) {
    if (reference_hz < AD9850_REFERENCE_HZ_MIN) reference_hz = AD9850_REFERENCE_HZ_MIN;
    if (reference_hz > AD9850_REFERENCE_HZ_MAX) reference_hz = AD9850_REFERENCE_HZ_MAX;
    ad9850_reference_hz = reference_hz;
}
uint32_t AD9850_GetReferenceHz(void) {
    return ad9850_reference_hz;
}

uint32_t AD9850_FreqHzToWord(uint32_t freq_hz) {
    return (uint32_t) (((uint64_t) freq_hz << 32) / ad9850_reference_hz);
}
uint32_t AD9850_FreqMilliHzToWord(uint64_t freq_millihz) {
    return (uint32_t) ((freq_millihz << 32) / ((uint64_t) ad9850_reference_hz * 1000));
}
uint32_t AD9850_AddWordOffset(uint32_t base_word, int32_t word_offset) {
    if (word_offset >= 0) {
        uint32_t offset = (uint32_t) word_offset;
        if (base_word > 0xffffffffu - offset) return 0xffffffffu;
        return base_word + offset;
    } else {
        uint32_t offset = (uint32_t) (-word_offset);
        if (base_word < offset) return 0;
        return base_word - offset;
    }
}


static void AD9850_WriteRight(uint32_t freq_word, unsigned char phase) {
    dds_ph = (phase & 0x1f) << 3;

    ad9850_wr_parrel(dds_ph);
    W_CKR_L; W_CKR_H; W_CKR_L;
    ad9850_wr_parrel((unsigned char) ((freq_word >> 24) & 0xff));
    W_CKR_L; W_CKR_H; W_CKR_L;
    ad9850_wr_parrel((unsigned char) ((freq_word >> 16) & 0xff));
    W_CKR_L; W_CKR_H; W_CKR_L;
    ad9850_wr_parrel((unsigned char) ((freq_word >> 8) & 0xff));
    W_CKR_L; W_CKR_H; W_CKR_L;
    ad9850_wr_parrel((unsigned char) (freq_word & 0xff));
    W_CKR_L; W_CKR_H; W_CKR_L;
}
static void AD9850_WriteLeft(uint32_t freq_word, unsigned char phase) {
    dds_ph = (phase & 0x1f) << 3;

    ad9850_wr_parrel(dds_ph);
    W_CKL_L; W_CKL_H; W_CKL_L;
    ad9850_wr_parrel((unsigned char) ((freq_word >> 24) & 0xff));
    W_CKL_L; W_CKL_H; W_CKL_L;
    ad9850_wr_parrel((unsigned char) ((freq_word >> 16) & 0xff));
    W_CKL_L; W_CKL_H; W_CKL_L;
    ad9850_wr_parrel((unsigned char) ((freq_word >> 8) & 0xff));
    W_CKL_L; W_CKL_H; W_CKL_L;
    ad9850_wr_parrel((unsigned char) (freq_word & 0xff));
    W_CKL_L; W_CKL_H; W_CKL_L;
}

void setup_AD9850_ControlWords(uint32_t freq_word1, uint32_t freq_word2,
    unsigned char phase1, unsigned char phase2) {
    AD9850_WriteRight(freq_word1, phase1);
    AD9850_WriteLeft(freq_word2, phase2);
    // 在全部输入完成后，通过一个Frequency_Update(FQ_UD)引脚的正脉冲来把数据写入
    FQ_UD_L; FQ_UD_H; FQ_UD_L;
}

void setup_AD9850_Hz(uint32_t freq_hz1, uint32_t freq_hz2,
    unsigned char phase1, unsigned char phase2) {
    setup_AD9850_ControlWords(AD9850_FreqHzToWord(freq_hz1), AD9850_FreqHzToWord(freq_hz2),
        phase1, phase2);
}
void setup_AD9850_HzWithOffset(uint32_t freq_hz1, uint32_t freq_hz2,
    int32_t word_offset1, int32_t word_offset2, unsigned char phase1, unsigned char phase2) {
    uint32_t freq_word1 = AD9850_AddWordOffset(AD9850_FreqHzToWord(freq_hz1), word_offset1);
    uint32_t freq_word2 = AD9850_AddWordOffset(AD9850_FreqHzToWord(freq_hz2), word_offset2);

    setup_AD9850_ControlWords(freq_word1, freq_word2, phase1, phase2);
}

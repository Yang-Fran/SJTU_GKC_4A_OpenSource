#ifndef AD9850_FUNC_H
#define AD9850_FUNC_H

// 板级 AD9850 DDS 驱动的公开接口。

// AD9850 使用 32 位相位累加器。每个参考时钟周期，相位累加器增加一次 FTW，
// 因此输出频率满足：
//   -- output_hz = FTW * reference_hz / 2^32
//   -- 这个说的是，一秒钟125MHz的晶振可以让累加器溢出多少圈，输出的频率就是多少
// 于是频率控制字的计算公式为：
//   -- FTW = output_hz * 2^32 / reference_hz
// FTW 增加 1 LSB 时，输出频率增加 reference_hz / 2^32；
// reference_hz=125000000Hz 时，1 FTW LSB 约等于 0.02910383Hz。
// 当reference增大一点点的时候，FTW不变的话，输出频率也随之增大

// 本板送入两片 AD9850 的公共标称参考时钟为 125MHz，因此驱动上电默认使用125000000Hz。
// 实际晶振会存在频偏，FREQ CAL 使用外部参考 100kHz 信号测量
// 99.9kHz 本振形成的拍频，并更新 reference_hz，使后续 Hz 到 FTW 的换算自动补偿公共参考时钟误差。
// 125MHz 是本板硬件标称值，不是所有 AD9850 的固定值。

// 更新时间：2026.06 氧化物

#include <stdint.h>

// ------------------------------
// AD9850 硬件层
// ------------------------------

// 初始化 AD9850 的控制引脚和 8 位并行数据引脚。
void ad9850_Port_Init(void);
// 复位两片 AD9850，并将更新和时钟控制线保持为低电平。
void ad9850_reset(void);
// 调试用代码，在CKL和RST上输入两组脉冲信号
void Clock_Pluse(void);

// 将一个字节输出到 AD9850 的 8 位并行数据总线。
// w: 待输出字节，按位映射到 GPE5、GPE3、GPE1、GPE8、GPE6、GPE4、GPE2、GPE0。
void ad9850_wr_parrel(unsigned char w);

// ------------------------------
// AD9850 驱动层
// ------------------------------

// 设置 DDS 实际参考时钟估计值，所有 Hz 到 FTW 的换算统一使用该值。
// reference_hz: 参考时钟 Hz，超出合理晶振范围时自动限幅。
void AD9850_SetReferenceHz(uint32_t reference_hz);
// 返回当前 DDS 参考时钟估计值，单位 Hz。
uint32_t AD9850_GetReferenceHz(void);

// 使用整数 Hz 配置两片 AD9850
// freq_hz1,2为整数频率，单位Hz; phase1,2为0..31整数，代表离散的32个相位值
void setup_AD9850_Hz(uint32_t freq_hz1, uint32_t freq_hz2,
    unsigned char phase1, unsigned char phase2);
// 直接用 32 位频率控制字配置两片 AD9850，供需要细微频率微调的闭环控制使用。
// freq_word1/freq_word2; 两路 AD9850 32 位频率控制字；phase1/phase2: 0..31 相位控制字。
void setup_AD9850_ControlWords(uint32_t freq_word1, uint32_t freq_word2,
    unsigned char phase1, unsigned char phase2);

// 使用整数 Hz 基础频率和动态 FTW 偏移配置两片 AD9850。
// word_offset1/word_offset2 仅用于 Costas 等运行期微调，不参与设备频率校准。
void setup_AD9850_HzWithOffset(uint32_t freq_hz1, uint32_t freq_hz2,
    int32_t word_offset1, int32_t word_offset2, unsigned char phase1, unsigned char phase2);

// ------------------------------
// 数值换算函数
// ------------------------------

// 将整数 Hz 频率转换为 AD9850 32 位 FTW。
uint32_t AD9850_FreqHzToWord(uint32_t freq_hz);
// 将毫 Hz 频率转换为 AD9850 32 位 FTW，供需要小数 Hz 的上层使用。
uint32_t AD9850_FreqMilliHzToWord(uint64_t freq_millihz);
// 对基础 FTW 叠加有符号动态偏移并做上下界保护。
uint32_t AD9850_AddWordOffset(uint32_t base_word, int32_t word_offset);


#endif

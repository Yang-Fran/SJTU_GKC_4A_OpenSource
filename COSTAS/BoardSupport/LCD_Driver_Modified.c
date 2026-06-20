#include "NUC1xx.h"
#include "DrvSYS.h"
#include "DrvSPI.h"
#include "DrvGPIO.h"

// 板级 SPI3 LCD 驱动，负责初始化 LCD 控制器并写入 ASCII 字符和字符串。
// 全局状态变量：SPI_PORT 是 SPI 寄存器基址查表，本文件实际只使用 eDRVSPI_PORT3。
// 外部数据：Ascii 是 8x16 ASCII 字模表，由 LB_002 库提供。
// 更新时间：2026.05 氧化物

static SPI_T *SPI_PORT[4] = {SPI0, SPI3, SPI2, SPI3};

extern char Ascii[];

// 1 使用 SysTick 做阻塞延时，按内部 22MHz RC 时钟估算。
// 2 us: 延时时间，单位微秒。
static void SysTimerDelay(uint32_t us) {
    SysTick->LOAD = us * 22; /* Assume the internal 22MHz RC used */
    SysTick->VAL = (0x00);
    SysTick->CTRL = (1 << SysTick_CTRL_CLKSOURCE_Pos) | (1 << SysTick_CTRL_ENABLE_Pos);

    /* Waiting for down-count to zero */
    while ((SysTick->CTRL & (1 << 16)) == 0);
}

// 1 初始化 SPI3 引脚和 LCD 控制器启动命令序列。
void LCD_Init(void) {
    SYSCLK->APBCLK.SPI3_EN = 1; // enable SPI3 clock
    SYS->IPRSTC2.SPI3_RST = 1; // reset SPI3
    SYS->IPRSTC2.SPI3_RST = 0;

    /* set GPIO to SPI mode*/
    SYS->GPDMFP.SPI3_SS0 = 1;
    SYS->GPDMFP.SPI3_CLK = 1;
    //SYS->GPDMFP.SPI3_MISO0  =1;
    SYS->GPDMFP.SPI3_MOSI0 = 1;

    SPI_PORT[eDRVSPI_PORT3]->CNTRL.CLKP = 1; // CLKP high idle
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.TX_BIT_LEN = 9; // 9-bit transfer
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.TX_NEG = 1; // transmit on negative edge
    SPI_PORT[eDRVSPI_PORT3]->DIVIDER.DIVIDER = 0X03; // clock divider

    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1; // enable slave select
    // Set BR
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0xEB;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1);
    // Set PM
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;

    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1;
    //outp32(SPI3_Tx0, 0x81);
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0x81;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1);
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0xa0;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1);
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;

    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1;
    //outp32(SPI3_Tx0, 0xC0);
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0xc0;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1);
    // Set Display Enable
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;

    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1;
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0XAF;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1);
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;
}

// 1 通过 SPI3 向 LCD 写入一个显示数据字节。
// 2 data: 写入 LCD 显存的数据字节，发送时会置位 SPI 的第 9 位。
void WriteData(unsigned char data) {
    // Write Data
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1; // chip select
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0x100 | data; // write data
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1); // wait for transfer complete
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;
}

// 1 设置 LCD 的页地址和列地址。
// 2 PA: 页地址；CA: 列地址。
void SetPACA(unsigned char PA, unsigned char CA) {
    // Set PA
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1;
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0xB0 | PA;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1); // wait for transfer complete
    // Set CA MSB
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;

    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1;
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0x10 | (CA >> 4) & 0xF;
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1); // wait for transfer complete
    // Set CA LSB
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;

    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 1;
    SPI_PORT[eDRVSPI_PORT3]->TX[0] = 0x00 | (CA & 0xF);
    SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY = 1;
    while (SPI_PORT[eDRVSPI_PORT3]->CNTRL.GO_BUSY == 1); // wait for transfer complete
    SPI_PORT[eDRVSPI_PORT3]->SSR.SSR = 0;
}

// 1 关闭蜂鸣器输出，将 GPB11 配置为输出并拉高。
void Disable_Buzzer(void) {
    DrvGPIO_Open(E_GPB, 11, E_IO_OUTPUT);
    DrvGPIO_SetBit(E_GPB, 11);
}

// 1 在 LCD 上绘制一个 8x16 ASCII 字符。
// 2 x/y: LCD 文本格坐标；ascii_word: 可打印 ASCII 字符码；inverse: 非 0 时反显。
void Draw_Word_8x16(unsigned char x, unsigned char y, unsigned char ascii_word, unsigned char inverse) {
    int i = 0, k = 0;
    unsigned char temp;
    k = (ascii_word - 32) * 16;

    for (i = 0; i < 8; i++) {
        SetPACA((x * 2), (129 - (y * 8) - i));
        temp = Ascii[k + i];
        if (inverse == 0) WriteData(temp);
        else WriteData(~temp);

        //      WriteData(temp);
    }

    for (i = 0; i < 8; i++) {
        SetPACA((x * 2) + 1, (129 - (y * 8) - i));
        temp = Ascii[k + i + 8];
        //    WriteData(temp);
        if (inverse == 0) WriteData(temp);
        else WriteData(~temp);
    }
}

// 1 从指定文本格开始，在 LCD 上打印一个以 '\0' 结尾的 ASCII 字符串。
// 2 line/column: 起始文本格坐标；str: 待显示字符串；inverse: 非 0 时反显。
void LCD_Print_String(unsigned char line, unsigned char column, unsigned char *str, unsigned char inverse) {
    int i = column;

    do {
        Draw_Word_8x16(line, i, *str++, inverse);
        i++;
        if (i > 16)
            break;
    } while (*str != '\0');
}

// 1 清空 LCD 全部显存。
void LCD_Clear_Panel(void) {
    int i = 0;
    /*CLEAR ALL PANNAL*/
    SetPACA(0x0, 0x0);

    for (i = 0; i < 132 * 8; i++) {
        WriteData(0x00);
    }
    WriteData(0x0f);
}

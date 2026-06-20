#include "DrvADC.h"
#include "DrvGPIO.h"

// 板级 ADC 采样模块，负责读取 ADC5/ADC6、保存当前采样值并生成滑动窗口平均值。
// 模块内部状态：ui32Windows 保存每个通道的窗口历史样本，i32Sums 保存窗口求和，ui8Index 保存当前写入位置。
// 公开输出：ADC0_Value 保存当前采样值，ADC0_Avg_Value 保存窗口平均值，由 adc_sample.h 对外声明。
// 更新时间：2026.05 氧化物

// 定义采样窗宽
#define ADC_WINDOW_SIZE 3
// 定义采样通道数，一直是2
#define ADC_CHANNEL_COUNT 2

// ADC采样的值
uint32_t ADC0_Value[ADC_CHANNEL_COUNT];
// 滑动平均后的ADC采样值
uint32_t ADC0_Avg_Value[ADC_CHANNEL_COUNT];
// 每完成一次双通道采样就递增，闭环据此只消费一次原始样本。
volatile uint16_t ADC0_SampleSequence;

// 滑动窗，存储全部数据
static uint32_t ui32Windows[ADC_CHANNEL_COUNT][ADC_WINDOW_SIZE];
// 滑动窗求和变量
static int32_t i32Sums[ADC_CHANNEL_COUNT];
// 窗中的循环指针
static uint8_t ui8Index[ADC_CHANNEL_COUNT];

// 1 触发 ADC5 和 ADC6 单次转换，并保存两个通道的当前采样值。
void ADC_SAMPLE(void) {
    DrvADC_SetADCChannel((1 << 5) | (1 << 6));
    DrvADC_StartConvert();
    while (!DrvADC_IsConversionDone());
    ADC0_Value[0] = DrvADC_GetConversionData(5);
    ADC0_Value[1] = DrvADC_GetConversionData(6);
    ADC0_SampleSequence++;
}

// 1 对 ADC5 和 ADC6 的采样值做滑动窗口平均。
void ADC_WINDOW(void) {
    uint8_t i;
    for (i = 0; i < ADC_CHANNEL_COUNT; i++) {
        i32Sums[i] += ADC0_Value[i];
        i32Sums[i] -= ui32Windows[i][ui8Index[i]];
        ui32Windows[i][ui8Index[i]] = ADC0_Value[i];
        ui8Index[i] = (ui8Index[i] + 1) % ADC_WINDOW_SIZE;
        ADC0_Avg_Value[i] = i32Sums[i] / ADC_WINDOW_SIZE;
    }
}

// 1 将 ADC 数值转换成 4 位十进制字符串。
// 2 adc: 待转换数值；str: 至少 5 字节的输出缓冲区，末尾写入 '\0'。
void ADC_ToString(uint32_t adc, unsigned char *str) {
    str[0] = '0' + adc / 1000;
    adc = adc % 1000;

    str[1] = '0' + adc / 100;
    adc = adc % 100;

    str[2] = '0' + adc / 10;
    adc = adc % 10;

    str[3] = '0' + adc;
    str[4] = '\0';
}

// 1 将 GPA5 和 GPA6 配置为 ADC 输入引脚。
void ADC_Port_Init(void) {
    DrvGPIO_Open(E_GPA, 5, E_IO_INPUT);
    DrvGPIO_Open(E_GPA, 6, E_IO_INPUT);
}

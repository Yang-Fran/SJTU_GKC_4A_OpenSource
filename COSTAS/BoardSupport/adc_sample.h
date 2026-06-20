#ifndef ADC_SAMPLE_H
#define ADC_SAMPLE_H

// 板级 ADC5/ADC6 采样、窗口平均、采样结果和显示字符串转换接口。
// 当前值和平均值由 adc_sample.c 定义，UI 和 Costas 环路可通过本头文件读取。
// 更新时间：2026.05 氧化物

#include <stdint.h>

extern uint32_t ADC0_Value[2];
extern uint32_t ADC0_Avg_Value[2];
// 原始双通道采样序号，闭环用它识别新样本和漏采样。
extern volatile uint16_t ADC0_SampleSequence;

void ADC_SAMPLE(void);
void ADC_WINDOW(void);
void ADC_ToString(uint32_t adc, unsigned char *str);
void ADC_Port_Init(void);

#endif

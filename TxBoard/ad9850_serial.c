/*
*********************************************************************************************************
*	模块名称 : AD9850驱动模块
*	文件名称 : bsp_ad9850.c
*	版    本 : V2.0
*	说    明 : 驱动DDS芯片AD9850，产生正弦波和方波（适配TI TM4C1294）
*	修改记录 :
*		版本号  日期        作者    说明
*       v2.0    2026-03-22      -       移植到TM4C1294平台
*********************************************************************************************************
*/

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"

#include "ad9850_serial.h"

#define AD9850_OSC_FREQ		125000000u	/* AD9850 模块外接的晶振频率, 单位Hz */

/*
	AD9850模块和TM4C1294连接方式：串行方式(需要6根线)
   【AD9850模块排针】 【TM4C1294引脚】
        VCC ----------- 3.3V
        GND ----------- GND
      RESET ----------- PE5
      W_CLK ----------- PC6
      FQ_UD ----------- PC5
         D7 ----------- PC4
  另外注意D2 ----------- GND
*/

/* 定义IO端口 （可根据实际硬件修改）*/
#define DDS_RESET_PORT     GPIO_PORTE_BASE
#define DDS_RESET_PIN      GPIO_PIN_5

#define DDS_W_CLK_PORT     GPIO_PORTC_BASE
#define DDS_W_CLK_PIN      GPIO_PIN_6

#define DDS_FQ_UD_PORT     GPIO_PORTC_BASE
#define DDS_FQ_UD_PIN      GPIO_PIN_5

#define DDS_D7_PORT        GPIO_PORTC_BASE
#define DDS_D7_PIN         GPIO_PIN_4

/* 引脚电平操作宏（TM4C版本）*/
#define DDS_RESET_1()      GPIOPinWrite(DDS_RESET_PORT, DDS_RESET_PIN, DDS_RESET_PIN)
#define DDS_RESET_0()      GPIOPinWrite(DDS_RESET_PORT, DDS_RESET_PIN, 0)

#define DDS_W_CLK_1()      GPIOPinWrite(DDS_W_CLK_PORT, DDS_W_CLK_PIN, DDS_W_CLK_PIN)
#define DDS_W_CLK_0()      GPIOPinWrite(DDS_W_CLK_PORT, DDS_W_CLK_PIN, 0)

#define DDS_FQ_UD_1()      GPIOPinWrite(DDS_FQ_UD_PORT, DDS_FQ_UD_PIN, DDS_FQ_UD_PIN)
#define DDS_FQ_UD_0()      GPIOPinWrite(DDS_FQ_UD_PORT, DDS_FQ_UD_PIN, 0)

#define DDS_D7_1()         GPIOPinWrite(DDS_D7_PORT, DDS_D7_PIN, DDS_D7_PIN)
#define DDS_D7_0()         GPIOPinWrite(DDS_D7_PORT, DDS_D7_PIN, 0)

static void AD9850_ResetToSerial(void);

/*
*********************************************************************************************************
*	函 数 名: AD9850_InitHard
*	功能说明: 初始化AD9850所连接的TM4C1294 GPIO端口
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void AD9850_InitHard(void)
{
    /* 1. 使能GPIO端口时钟 */
    // 使能PORTA时钟 (SYSCTL_PERIPH_GPIOC)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    // 使能PORTG时钟 (SYSCTL_PERIPH_GPIOE)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    
    /* 等待端口时钟稳定 */
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC));
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));
    

    
    /* 2. 配置GPIO为推挽输出模式 */
    GPIOPinTypeGPIOOutput(DDS_RESET_PORT, DDS_RESET_PIN);
    GPIOPinTypeGPIOOutput(DDS_W_CLK_PORT, DDS_W_CLK_PIN);
    GPIOPinTypeGPIOOutput(DDS_FQ_UD_PORT, DDS_FQ_UD_PIN);
    GPIOPinTypeGPIOOutput(DDS_D7_PORT, DDS_D7_PIN);
	
    /* 3. 初始化引脚为低电平 */
    DDS_RESET_0();
    DDS_W_CLK_0();
    DDS_FQ_UD_0();
    DDS_D7_0();
    
    
    /* 4. 复位进入串口模式 */
    AD9850_ResetToSerial();
}

/*
*********************************************************************************************************
*	函 数 名: AD9850_ResetToSerial
*	功能说明: 复位AD9850，之后为串口模式
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void AD9850_ResetToSerial(void)
{
    /* AD9850 进入串口模式的时序（TM4C版本）*/
    DDS_W_CLK_0();
    DDS_FQ_UD_0();
    DDS_RESET_0();

    /* 产生一个复位脉冲 */
    DDS_RESET_1();
    DDS_RESET_0();

    /* 产生一个W_CLK脉冲 */
    DDS_W_CLK_1();
    DDS_W_CLK_0();

    /* 产生一个FQ_UP脉冲 */
    DDS_FQ_UD_1();
    DDS_FQ_UD_0();

    /* 之后，AD9850 进入串口模式 */
}

/*
*********************************************************************************************************
*	函 数 名: AD9850_WriteCmd
*	功能说明: 按串口协议，发送40bit控制命令
*	形    参：_ucPhase ：相位参数(一般填0）；_dOutFreq ：频率参数(浮点数)，单位Hz，可以输入 0.01Hz
*	返 回 值: 无
*********************************************************************************************************
*/
void AD9850_WriteCmd(uint8_t _ucPhase, double _dOutFreq)
{
    uint32_t ulFreqWord;
    uint8_t i;

    /* 频率控制字计算（逻辑完全不变）*/
    ulFreqWord =  (uint32_t)((_dOutFreq* 4294967296.0) / AD9850_OSC_FREQ);

    /* 写32bit 频率字（低位先传）*/
    for (i = 0; i < 32; i++)
    {
        if (ulFreqWord & 0x00000001)
        {
            DDS_D7_1();
        }
        else
        {
            DDS_D7_0();
        }
        ulFreqWord >>= 1;
        DDS_W_CLK_1();
        DDS_W_CLK_0();
    }

    /* 发送第5个字节；相位参数。 一般填 0 */
    for (i = 0; i < 8; i++)
    {
        if (_ucPhase & 0x00000001)
        {
            DDS_D7_1();
        }
        else
        {
            DDS_D7_0();
        }
        _ucPhase >>= 1;
        DDS_W_CLK_1();
        DDS_W_CLK_0();
    }

    /* FQ_UD脉冲更新寄存器 */
    DDS_FQ_UD_1();
    DDS_FQ_UD_0();
}

void AD98850_Output_0(void) {
    DDS_RESET_0();
    DDS_RESET_1();
    DDS_RESET_0();

    DDS_W_CLK_0();
    DDS_W_CLK_1();
    DDS_W_CLK_0();

    DDS_FQ_UD_0();
    DDS_FQ_UD_1();
    DDS_FQ_UD_0();
}

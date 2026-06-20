//*****************************************************************************
//
// Copyright:
// File name: dds_ser_main.c
// Description: 
//    1. 开机或复位后，在主程序中启动AD9850,以串行模型、控制输出频率100KHz，相位为0的正弦波
//    2. 支持使用按键控制状态机，在0,ASK,FSK,PSK,DPSK,QPSK中切换
//    3. 2ASK,2PSK状态的工作频率是100KHz
//		 2FSK在95/105KHz中切换
// Pin Map:
//		需要将PE5->DDS_RESET, PC456->DDS_D7,FQ_UD,W_CLK连接
//		在PE123测试ABC信号
// Author:
// Version:
// Date：
// History：

//*****************************************************************************

//*****************************************************************************
//
// 头文件
//
//*****************************************************************************
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"        // 基址宏定义
#include "inc/hw_types.h"         // 数据类型宏定义，寄存器访问函数
#include "driverlib/debug.h"      // 调试用
#include "driverlib/gpio.h"       // 通用IO口宏定义
#include "driverlib/pin_map.h"    // TM4C系列MCU外围设备管脚宏定义
#include "driverlib/sysctl.h"	  // 系统控制定义
#include "driverlib/systick.h"    // SysTick Driver 原型
#include "driverlib/interrupt.h"  // NVIC Interrupt Controller Driver 原型

#include "tm1638.h"               // 与控制TM1638芯片有关的函数
#include "ad9850_serial.h"


#define SYSTICK_FREQUENCY		1000	// SysTick频率为1000Hz，即循环定时周期1ms
#define V_T100ms	100                  // 0.1s软件定时器溢出值，100个1ms
#define V_T500ms	500                  // 0.5s软件定时器溢出值，500个1ms

// 初始化函数组

void GPIOInit(void);
void SysTickInit(void);
void DevicesInit(void);

void CheckButton(void);
void StateMachineTran(void);
void StateMachineInvr(void);
void StateMachineInto(uint8_t state);

void AD9850_WorkState(double phase, double freq);
void SetMSeries(bool s0, bool s1, bool s2);

// ===========================================================
// 记时变量

// 系统时钟频率
uint32_t ui32SysClock;
// 软件定时器计数
uint8_t clk100ms_unstop = 0;
uint8_t clk500ms_unstop = 0;
// 软件定时器溢出标志
uint8_t clk100ms_unstop_flag = 0;
uint8_t clk500ms_unstop_flag = 0;
// ===========================================================


// ===========================================================
// 数码管显示变量

// 8位数码管显示的数字或字母符号
// 注：板上数码位从左到右序号排列为4、5、6、7、0、1、2、3
uint8_t digit[8]={'1','0','0','0',' ',' ',' ','0'};
// 8位小数点 1亮  0灭
// 注：板上数码位小数点从左到右序号排列为4、5、6、7、0、1、2、3
uint8_t pnt = 0x00;
uint8_t unused_compensation_uint8 = 0;
// 8个LED指示灯状态，0灭，1亮
// 注：板上指示灯从左到右序号排列为7、6、5、4、3、2、1、0
//     对应元件LED8、LED7、LED6、LED5、LED4、LED3、LED2、LED1
uint8_t led[] = {1, 1, 1, 1, 1, 1, 1, 1};



// 当前按键值
uint8_t key_code = 0;
// ===========================================================



// ===========================================================
// 状态机变量

// 发射机的状态：0-100kHz正弦, 1-ASK, 2-FSK, 3-PSK, 4-DPSK, 5-QPSK
volatile uint8_t state_machine;
#define STATE_CARRIER 0
#define STATE_ASK 1
#define STATE_FSK 2
#define STATE_PSK 3
#define STATE_DPSK 4
#define STATE_QPSK 5
#define STATE_MACHINE_CNT 6
bool current_bit;

// ===========================================================


// ===========================================================
// M序列定义

// m序列生成变量
uint8_t m_ser_counter;
// 该m序列是由→{111}→为初态，s[k+3]=s[k]+s[k+1](1+x+x3)构造的三阶m序列
bool m_ser_output[7]={1,1,1,0,0,1,0};
bool m_ser_diff_output[7] = {0,1,0,0,0,1,1};
bool m_ser_base_clk;
// QPSK使用固定8bit序列00101101，每两个连续bit组成一个符号
bool qpsk_output[8] = {0,0,1,0,1,1,0,1};
volatile uint8_t qpsk_bit_counter;
// m序列标志位
#define M_SER_PORT        GPIO_PORTE_BASE
#define M_SER_A_PIN         GPIO_PIN_1
#define M_SER_B_PIN         GPIO_PIN_2
#define M_SER_C_PIN         GPIO_PIN_3
void M_SER_A(bool s) { GPIOPinWrite(M_SER_PORT, M_SER_A_PIN, s ? M_SER_A_PIN : 0); }
void M_SER_B(bool s) { GPIOPinWrite(M_SER_PORT, M_SER_B_PIN, s ? M_SER_B_PIN : 0); }
void M_SER_C(bool s) { GPIOPinWrite(M_SER_PORT, M_SER_C_PIN, s ? M_SER_C_PIN : 0); }
// ===========================================================

// ===========================================================
// dds驱动程序

// dds当前状态
double dds_freq_prev;
double dds_phase_prev;
// m序列DDS参数表
// 对应的是0, ASK, FSK, PSK, DPSK, QPSK
double dds_phase_0[STATE_MACHINE_CNT] = {0,      0,     0,      0,      0, 0};
double dds_phase_1[STATE_MACHINE_CNT] = {0,      0,     0,    180,    180, 0};
double dds_freq_0[STATE_MACHINE_CNT] = {100000,  0, 95000, 100000, 100000, 100000};
double dds_freq_1[STATE_MACHINE_CNT] = {100000, 100000, 105000, 100000, 100000, 100000};
// Gray映射：00->0度，01->90度，11->180度，10->270度
double qpsk_phase[4] = {0, 90, 270, 180};
// ===========================================================


// ===========================================================
// 按钮变量

// 按钮的前态
bool btn_left_pre, btn_right_pre, btn_swap_pre;
// 按钮标志位
bool btn_left_flag, btn_right_flag;
bool btn_swap_flag;
// ===========================================================

/// 全局记数变量
uint8_t i, j, temp;

int main(void) {

	// 初始化时序
	DevicesInit();            //  MCU器件初始化
	AD9850_InitHard();        //  AD9850初始化
	GPIOPinTypeGPIOOutput(M_SER_PORT, M_SER_A_PIN);
	GPIOPinTypeGPIOOutput(M_SER_PORT, M_SER_B_PIN);
	GPIOPinTypeGPIOOutput(M_SER_PORT, M_SER_C_PIN);

	while (clk100ms_unstop < 3);   // 延时>60ms,等待TM1638上电完成
	TM1638_Init();	          // 初始化TM1638

 	// 板初始工作状态
	state_machine = 0;

	dds_freq_prev = 100000;
	dds_phase_prev = 0;
	AD9850_WriteCmd(0,100000);

	m_ser_counter = 6; //第一次更新后会变成0
	m_ser_base_clk = false;
	qpsk_bit_counter = 7; // Q模式下第一个bit从序列下标0开始

	SetMSeries(true, true, true);
	
	while (1) {
		// 检查0.1秒定时是否到
		if (clk100ms_unstop_flag == 1) {
			clk100ms_unstop_flag = 0;
			// 刷新全部数码管和LED指示灯
			TM1638_RefreshDIGIandLED(digit, pnt, led);
		}
		// 检查0.5秒定时是否到
		if (clk500ms_unstop_flag == 1) {
			clk500ms_unstop_flag = 0;
			// 该功能暂时没有使用
		}



		// 更新状态机
		if (btn_left_flag) {
			btn_left_flag = false;
			StateMachineInvr();
		}
		if (btn_right_flag) {
			btn_right_flag = false;
			StateMachineTran();
		}
		if (btn_swap_flag) {
			btn_swap_flag = false;
			// 这个功能暂时没有用到
		}
		for (i=0;i<8;i++) led[i] = 1;
		led[7-state_machine] = 0;
		switch (state_machine) {
			case STATE_CARRIER: {
				// 100kHz sine mode
				digit[7] = 'o';
				break;
			}
			case STATE_ASK: {
				// ASK mode
				digit[7] = 'A';
				break;
			}
			case STATE_FSK: {
				digit[7] = 'F';
				break;
			}
			case STATE_PSK: {
				digit[7] = 'P';
				break;
			}
			case STATE_DPSK: {
				digit[7] = 'd';
				break;
			}
			case STATE_QPSK: {
				digit[7] = 'Q';
				break;
			}
			default: break;
		}

	}
	
}


// ===========================================================

// 函数原型：void SysTick_Handler(void)
// 函数功能：SysTick中断服务程序
//          更新各记时器，并更新数码管和按键状态
void SysTick_Handler(void) {
	// 0.1秒钟软定时器计数
	if (++clk100ms_unstop >= V_T100ms) {
		clk100ms_unstop_flag = 1;    // 当0.1秒到时，溢出标志置1
		clk100ms_unstop = 0;
	}

	// 0.5秒钟软定时器计数
	if (++clk500ms_unstop >= V_T500ms) {
		clk500ms_unstop_flag = 1;    // 当0.5秒到时，溢出标志置1
		clk500ms_unstop = 0;
	}

	// 检查当前键盘输入，0 代表无键操作，1-9 表示有对应按键
	key_code = TM1638_Readkeyboard();
	CheckButton();

	// 更新输出变量
	m_ser_base_clk = !m_ser_base_clk;
	// B端输出1bit时钟
	if (state_machine != STATE_CARRIER)
		M_SER_B(m_ser_base_clk);
	else if (state_machine == STATE_CARRIER)
		M_SER_B(false);
	// 输出时钟的上升沿
	if (m_ser_base_clk) {

		if (state_machine == STATE_CARRIER) {
			M_SER_A(false);
			M_SER_C(false);
			AD9850_WorkState(0, 100000);
			return;
		}

		if (state_machine == STATE_QPSK) {
			// QPSK模式：A为00101101序列，B为1bit时钟，C为8bit周期同步
			qpsk_bit_counter = (qpsk_bit_counter == 7) ? 0 : qpsk_bit_counter + 1;
			current_bit = qpsk_output[qpsk_bit_counter];
			M_SER_A(current_bit);
			M_SER_C(qpsk_bit_counter == 0);

			if ((qpsk_bit_counter & 1) == 0) {
				uint8_t qpsk_symbol =
					(current_bit << 1) | qpsk_output[qpsk_bit_counter + 1];
				AD9850_WorkState(qpsk_phase[qpsk_symbol], 100000);
			}
		} else {
			// 其他调制模式：A为原始m序列，B为1bit时钟，C为m序列周期同步
			m_ser_counter = (m_ser_counter == 6) ? 0 : m_ser_counter + 1;
			current_bit = m_ser_output[m_ser_counter];
			M_SER_A(current_bit);
			M_SER_C(m_ser_counter==0);
		}

		// D端输出 - 更新DDS
		if (state_machine == STATE_DPSK)
			m_ser_diff_output[m_ser_counter] ?
				AD9850_WorkState(dds_phase_1[state_machine], dds_freq_1[state_machine]) :
				AD9850_WorkState(dds_phase_0[state_machine], dds_freq_0[state_machine]);
		else if (state_machine != STATE_QPSK)
			m_ser_output[m_ser_counter] ?
				AD9850_WorkState(dds_phase_1[state_machine], dds_freq_1[state_machine]) :
				AD9850_WorkState(dds_phase_0[state_machine], dds_freq_0[state_machine]);

	}
}
// ===========================================================


// ===========================================================
// 状态机和按钮更新

// 检查按钮是否按下，每次在Systick中更新
// 按钮包括4,5,6号，仅检查上升沿
void CheckButton(void) {
	if (key_code==0) {
		btn_left_pre = btn_right_pre = btn_swap_pre = false;
	}
	if (key_code == 4 && btn_left_pre == false) {
		btn_left_flag = true;
		btn_left_pre = true;
	}
	if (key_code == 5 && btn_swap_pre == false) {
		btn_swap_flag = true;
		btn_swap_pre = true;
	}
	if (key_code == 6 && btn_right_pre == false) {
		btn_right_flag = true;
		btn_right_pre = true;
	}
}

// 状态机切换至次态
void StateMachineTran(void) {
	if (state_machine == STATE_MACHINE_CNT - 1) StateMachineInto(STATE_CARRIER);
	else StateMachineInto(state_machine + 1);
}

// 状态机切换至前态
void StateMachineInvr(void) {
	if (state_machine == STATE_CARRIER) StateMachineInto(STATE_MACHINE_CNT - 1);
	else StateMachineInto(state_machine - 1);
}

// 状态机切换至指定态
void StateMachineInto(uint8_t state) {
	if (state < STATE_MACHINE_CNT) {
		if (state == STATE_QPSK || state_machine == STATE_QPSK) {
			qpsk_bit_counter = 7;
			M_SER_A(false);
			M_SER_B(false);
			M_SER_C(false);
		}
		state_machine = state;
		if (state_machine == STATE_CARRIER) {
			M_SER_A(false);
			M_SER_B(false);
			M_SER_C(false);
			AD9850_WorkState(0, 100000);
		}
	}
}
// ===========================================================


// ===========================================================
// m序列

// 更新AD9850的状态，会根据自己的状态判断需要如何发指令
void AD9850_WorkState(double phase, double freq) {
	// 它这个给的参考函数非常阴
	// 我们把真实的角度值映射到一个5bit数
	uint8_t phase_code = (int)(phase/11.25);
	// 没有说明的是，它的最低两位应当是始终保持0的控制字，倒数第三位是0，代表不省电（1的话会进省电模式）
	// 对应内容参考AD9850 datasheet Page12表格
	phase_code <<= 3;
	// 检查是否需要更新
	if (phase == dds_phase_prev && freq == dds_freq_prev) return ;
	// 更新变量，并输出控制字
	dds_phase_prev = phase;
	dds_freq_prev = freq;
	if (freq == 0) {
		// 说明是在输出0
		AD98850_Output_0();
	} else {
		AD9850_WriteCmd(phase_code, freq);
	}
}

// 根据初始值设计M序列
void SetMSeries(bool s0, bool s1, bool s2) {
	bool tmp0 = s0, tmp1 = s1, tmp2 = s2;
	bool t;
	for (i = 0; i < 7; i++) {
		t = tmp1 ^ tmp2;
		m_ser_output[i] = tmp2;
		tmp2 = tmp1;
		tmp1 = tmp0;
		tmp0 = t;
	}
	m_ser_diff_output[0] = !m_ser_output[0];
	for (i = 1; i < 7; i++) {
		m_ser_diff_output[i] = m_ser_output[i] ^ m_ser_diff_output[i-1];
	}
}
// ===========================================================

/// 初始化函数组

// 函数原型：void GPIOInit(void)
// 函数功能：GPIO初始化。使能PortK，设置PK4,PK5为输出；使能PortM，设置PM0为输出。
//          （PK4连接TM1638的STB，PK5连接TM1638的DIO，PM0连接TM1638的CLK）
void GPIOInit(void) {
	//配置TM1638芯片管脚
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);		  // 使能端口 K	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK)){}; // 等待端口 K准备完毕		
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);		  // 使能端口 M	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOM)){}; // 等待端口 M准备完毕		
	
    // 设置端口 K的第4,5位（PK4,PK5）为输出引脚		PK4-STB  PK5-DIO
	GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_4|GPIO_PIN_5);
	// 设置端口 M的第0位（PM0）为输出引脚   PM0-CLK
	GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_0);

}


// 函数原型：SysTickInit(void)
// 函数功能：设置SysTick中断
void SysTickInit(void) {
	SysTickPeriodSet(ui32SysClock/SYSTICK_FREQUENCY); // 设置心跳节拍,定时周期20ms
	SysTickEnable();  			// SysTick使能
	SysTickIntEnable();			// SysTick中断允许
}

// 函数原型：void DevicesInit(void)
// 函数功能：CU器件初始化，包括系统时钟设置、GPIO初始化和SysTick中断设置
void DevicesInit(void) {
	// 使用外部25MHz主时钟源，经过PLL，然后分频为20MHz
	ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN | 
	                                   SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 
	                                   20000000);

	GPIOInit();             // GPIO初始化
    SysTickInit();          // 设置SysTick中断
    IntMasterEnable();		// 总中断允许
}

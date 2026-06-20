#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NUC1xx.h"
#include "DrvGPIO.h"
#include "DrvSYS.h"
#include "DrvSPI.h"
#include "DrvADC.h"
#include "DrvTIMER.h"

#include "Scan_6Key.h"
#include "AD9850_func.h"
#include "UIFUNC0728.h"
#include "adc_sample.h"
#include "app_state.h"
#include "LCD_Driver_Modified.h"

// 项目主入口文件，负责系统时钟、GPIO、ADC、LCD、键盘、AD9850 和 Timer0 的初始化。
// Timer0 周期任务负责按键扫描、ADC 采样、100ms 节拍和 1 秒节拍。
// 全局状态变量：clock100ms/clock1s 记录 0.2ms tick 计数，key_display 保存调试显示字符。
// flag 变量：clock100ms_flag/clock1s_flag 由 Timer0 回调置位，在 main 循环中清零。
// 调试开关：TEST_AD9850_RESET_1S 为 AD9850 1 秒复位测试开关，当前关闭。

#define V_T1s 5000
#define V_T100ms 500

// Timer0 每 0.2ms 执行一次，连续 100 次一致约等于 20ms 稳定。
#define KEY_DEBOUNCE_TICKS 100
// 加减键长按重复周期约 500ms。
#define KEY_REPEAT_TICKS 2500

// 调试用代码，测试AD9850 reset功能：该宏定义设置为1时，每1秒发送一次RESET信号
#define TEST_AD9850_RESET_1S 0

// 板上的调试输出
#define C12_LED_H    DrvGPIO_SetBit(E_GPC,12)   // LED off
#define C12_LED_L    DrvGPIO_ClrBit(E_GPC,12)   // LED on
#define PC13_H       DrvGPIO_SetBit(E_GPC,13)   // LED6 off / Timer0 probe high
#define PC13_L       DrvGPIO_ClrBit(E_GPC,13)   // LED6 on / Timer0 probe low

unsigned int clock1s = 0;
volatile unsigned int clock1s_flag = 0;
unsigned int clock100ms = 0;
volatile unsigned int clock100ms_flag = 0;

unsigned char key_display[] = "0";

// Timer0 写入按钮事件，UI 通过 Button_TakeFlags 读取并清零。
uint8_t key = 0;
volatile static uint8_t button_flags = 0;
static uint8_t timer_probe_state = 0;

// ----- 初始化函数段 -----

// 1 初始化芯片时钟、ADC、板级 I/O 和 Timer0，并启动 Timer0。
void Init_Devices(void);
// 1 初始化板级 I/O 外设，包括 AD9850、LCD、键盘、调试引脚和 ADC 输入引脚。
void Port_Init(void);

// 1 初始化 Timer0，使其以 5kHz 周期模式运行，并每个 tick 调用 Timer0_Callback。
void Timer0_Init(void);
// 1 Timer0 周期回调，执行 0.2ms 周期任务和节拍累计。
void Timer0_Callback(void);

// ----- 调试用/功能性函数段 -----

// 1 调试用：每秒复位 AD9850 后重新写入固定测试输出。
static void AD9850_Reset_1s_TestTick(void);
// 1 将 0..9999 范围内的整数转换成 4 位十进制字符串。
// 2 i: 待转换整数；istr: 至少 5 字节的输出缓冲区，末尾写入 '\0'。
void integer_to_StringArray(int i, unsigned char *istr);
// 1 对原始按键值做连续采样消抖，只有连续稳定后才更新输出值。
// 2 raw_key: 当前一次键盘扫描返回值，0 表示无按键。
static uint8_t DebounceKeySample(uint8_t raw_key);
// 1 根据稳定后的按键状态生成单次按下事件。
// 2 is_pressed: 当前按键是否处于按下状态；state/prestate: 单个按键的历史状态；flag: 要置位的按钮事件位。
static void Button_UpdatePressEvent(uint8_t is_pressed, uint8_t *state, uint8_t *prestate, uint8_t flag);
// 1 根据稳定后的按键状态生成单次按下事件，并在长按时周期性重复触发。
// 2 timer: 长按重复计数器；flag: 要置位的按钮事件位。
static void Button_UpdateRepeatEvent(uint8_t is_pressed, uint8_t *state, uint8_t *prestate, uint16_t *timer, uint8_t flag);


// 1 程序入口，完成系统初始化后进入 UI 主循环。
int main(void) {
    // 初始化
    Init_Devices();
    DrvSYS_Delay(2000);     // 延迟2000us，等待上电完成
    LCD_Init();                // 初始化LCD屏
    init_act();                // 这是在做初始化状态机界面
    ad9850_reset();            // AD9850 RESET，即初始化时序

    // 初始化结束，点个灯
    DrvSYS_Delay(2000);

    C12_LED_L;
    DrvSYS_Delay(2000);
    C12_LED_H;

    // CKL，RST上做一个验证性脉冲
    Clock_Pluse();

    DrvTIMER_EnableInt(E_TMR0); // Timer0 中断使能

    // 初始化一个工作状态：双路100kHz，相位差0 vs 270(第24挡)
    setup_AD9850_Hz(100000UL, 100000UL, 0, 24); // Test output


    // 主循环
    while (1) {

        // 闭环先消费新的 ADC 原始样本，再处理可能较慢的按键和 UI 操作。
        AppState_Task();
        ui_state_proc();

        // 100ms tick
        if (clock100ms_flag == 1) {
            clock100ms_flag = 0;
            UI_RuntimeDisplayTask();
        }

        // 1s tick
        // 只用于调试，目前无功能
        if (clock1s_flag == 1) {
            clock1s_flag = 0;
            AD9850_Reset_1s_TestTick();
            C12_LED_H;
            C12_LED_L;
            C12_LED_L;
            C12_LED_L;
            C12_LED_L;
            C12_LED_H;
        }
    }
}






void Init_Devices(void) {
    SYSCLK->APBCLK.WDT_EN = 0; // Disable WDT clock source

    // Enable the external 12MHz oscillator oscillation
    DrvSYS_SetOscCtrl(E_SYS_XTL12M, 1);
    // HCLK clock source. 0: external 12MHz; 4:internal 22MHz RC oscillator
    DrvSYS_SelectHCLKSource(0);
    // HCLK clock frequency = 12M/(0+1)=12M
    DrvSYS_SetClockDivider(E_SYS_HCLK_DIV, 0);

    DrvADC_Open(ADC_SINGLE_END, ADC_SINGLE_CYCLE_OP,
        (1 << 5) | (1 << 6), INTERNAL_HCLK, 0);
    // ADC channel 5/6, ADC clock = 12MHz

    Port_Init();
    Timer0_Init();

    DrvTIMER_Start(E_TMR0);
}
void Port_Init(void) {
    ad9850_Port_Init();
    LCD_Init();
    OpenKeyPad();

    DrvGPIO_Open(E_GPC, 12, E_IO_OUTPUT); // LED
    DrvGPIO_Open(E_GPC, 13, E_IO_OUTPUT); // LED6 / Timer0 timing probe
    PC13_L;

    ADC_Port_Init();
}
void Timer0_Init(void) {
    DrvTIMER_Init();
    DrvTIMER_Open(E_TMR0, 5000, E_PERIODIC_MODE);
    DrvTIMER_SetTimerEvent(E_TMR0, 1, (TIMER_CALLBACK) Timer0_Callback, 0);
}
void Timer0_Callback(void) {
    // 每个 Timer0 tick 翻转一次，5kHz 中断应在 PC13/LED6 输出约 2.5kHz 方波。
    timer_probe_state = !timer_probe_state;
    if (timer_probe_state) PC13_H;
    else PC13_L;

    // 按钮逻辑处理
    Button_Task();

    // ADC采样后用Window函数取平均
    ADC_SAMPLE();
    ADC_WINDOW();

    if (++clock100ms >= V_T100ms) {
        clock100ms_flag = 1;
        clock100ms = 0;
    }

    if (++clock1s >= V_T1s) {
        clock1s_flag = 1;
        clock1s = 0;
    }

}

static void AD9850_Reset_1s_TestTick(void) {
    if (!TEST_AD9850_RESET_1S) return ;
    ad9850_reset();
    // Reload output config after reset for deterministic waveform.
    setup_AD9850_Hz(100000UL, 100000UL, 0, 24);
    //Clock_Pluse();
}
void integer_to_StringArray(int i, unsigned char *istr) {
    uint32_t j;
    j = i / 1000;
    istr[0] = '0' + j;
    i = i - j * 1000;
    j = i / 100;
    istr[1] = '0' + j;
    i = i - j * 100;
    j = i / 10;
    istr[2] = '0' + j;
    i = i - j * 10;
    istr[3] = '0' + i;
    istr[4] = '\0';
}
static uint8_t DebounceKeySample(uint8_t raw_key) {
    static uint8_t last_raw = 0;
    static uint8_t stable_key = 0;
    static uint8_t same_count = 0;

    if (raw_key == last_raw) {
        if (same_count < KEY_DEBOUNCE_TICKS) {
            same_count++;
        }
    } else {
        last_raw = raw_key;
        same_count = 1;
    }

    if (same_count >= KEY_DEBOUNCE_TICKS) {
        stable_key = raw_key;
    }

    return stable_key;
}
void Button_Task(void) {
    static uint8_t enter_state = 1, enter_prestate = 1;
    static uint8_t down_state = 1, down_prestate = 1;
    static uint8_t up_state = 1, up_prestate = 1;
    static uint8_t increase_state = 1, increase_prestate = 1;
    static uint8_t decrease_state = 1, decrease_prestate = 1;
    static uint16_t increase_timer = 0;
    static uint16_t decrease_timer = 0;

    key = DebounceKeySample(ScanKey());

    Button_UpdatePressEvent(key == 3, &enter_state, &enter_prestate, BUTTON_FLAG_ENTER);
    Button_UpdatePressEvent(key == 2, &down_state, &down_prestate, BUTTON_FLAG_DOWN);
    Button_UpdatePressEvent(key == 1, &up_state, &up_prestate, BUTTON_FLAG_UP);
    Button_UpdateRepeatEvent(key == 5, &increase_state, &increase_prestate, &increase_timer, BUTTON_FLAG_INCREASE);
    Button_UpdateRepeatEvent(key == 4, &decrease_state, &decrease_prestate, &decrease_timer, BUTTON_FLAG_DECREASE);
}

uint8_t Button_TakeFlags(void) {
    uint8_t flags;

    // __disable_irq();
    flags = button_flags;
    button_flags = 0;
    // __enable_irq();

    return flags;
}
static void Button_UpdatePressEvent(uint8_t is_pressed, uint8_t *state, uint8_t *prestate, uint8_t flag) {
    if (is_pressed) {
        *prestate = *state;
        *state = 0;
        if (*prestate == 1) {
            button_flags |= flag;
        }
    } else {
        *prestate = *state;
        *state = 1;
    }
}
static void Button_UpdateRepeatEvent(uint8_t is_pressed, uint8_t *state, uint8_t *prestate, uint16_t *timer, uint8_t flag) {
    if (is_pressed) {
        *prestate = *state;
        *state = 0;
        if (*prestate == 1) {
            button_flags |= flag;
            *timer = 0;
        } else if (*prestate == 0) {
            if (++(*timer) >= KEY_REPEAT_TICKS) {
                button_flags |= flag;
                *timer = 0;
            }
        }
    } else {
        *prestate = *state;
        *state = 1;
        *timer = 0;
    }
}

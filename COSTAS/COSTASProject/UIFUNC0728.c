#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NUC1xx.h"
#include "DrvGPIO.h"
#include "DrvSYS.h"
#include "DrvSPI.h"
#include "DrvTIMER.h"

#include "app_state.h"
#include "costas_loop.h"
#include "adc_sample.h"
#include "LCD_Driver_Modified.h"
#include "UIFUNC0728.h"

// ======================= 常量字符串 ===========================

// 状态机0 -- 欢迎界面
unsigned char a0_s0[]="Costas Loop";
// 状态机0-欢迎界面-WELCOME
unsigned char a0_s1[]="WELCOME";
// 状态机0-欢迎界面-按任意键
unsigned char a0_s2[]="PRESS ANY KEY";

// 状态机1 -- 选择模式
unsigned char a1_s0[]="CHOOSE OP MODE";
// 状态机1-选择模式-模式1(状态机2)两路独立信号
unsigned char a1_s1[]="DUO INDEP SIGNAL";
// 状态机1-选择模式-模式2(状态机4)两路相差信号
unsigned char a1_s2[]="90D PHASE SIGNAL";
// 状态机1-选择模式-校准模式
unsigned char a1_s3[]="CALIBRATION";

// 状态机2 -- 两路独立信号 -- 调试界面
unsigned char a2_s0[]="DUO INDEP SIGNAL";
// 状态机2-两路独立信号-第一路信号字面量
unsigned char a2_s1[]="FREQ 1"; unsigned char a2_s2[]="KHZ";
// 状态机2-两路独立信号-第二路信号字面量
unsigned char a2_s3[]="FREQ 2"; unsigned char a2_s4[]="KHZ";
// 状态机2-两路独立信号-运行/停止字面量
unsigned char a2_s5[]="RUN"; unsigned char a2_s6[]="BACK";
// 状态机2-两路独立信号-第一路频率示数（s7~9，三位）
unsigned char a2_s7[]="1", a2_s8[]="0", a2_s9[]="0";
// 状态机2-两路独立信号-第二路频率示数（s10~12，三位）
unsigned char a2_s10[]="1", a2_s11[]="0", a2_s12[]="0";

// 状态机3 -- 两路独立信号 -- 运行界面
unsigned char a3_s0[]="DUO INDEP SIGNAL";
// 状态机3-两路独立信号-第一路信号字面量
unsigned char a3_s1[]="FREQ 1", a3_s2[]="KHZ";
// 状态机3-两路独立信号-第二路信号字面量
unsigned char a3_s3[]="FREQ 2", a3_s4[]="KHZ";
// 状态机3-两路独立信号-运行/停止字面量
unsigned char a3_s5[]="RUNNING", a3_s6[]="BACK";
// 状态机3-两路独立信号-第一路频率示数（s7~9，三位）
unsigned char a3_s7[]="1", a3_s8[]="0", a3_s9[]="0";
// 状态机3-两路独立信号-第二路频率示数（s10~12，三位）
unsigned char a3_s10[]="1", a3_s11[]="0", a3_s12[]="0";

// 状态机4 -- 两路相差信号 -- 调试界面
unsigned char a4_s0[]="90D PHASE SIGNAL";
// 状态机4-两路相差信号-频率/相位字面量
unsigned char a4_s1[]="FREQ", a4_s2[]="KHz", a4_s3[]="PHASE";
// 状态机4-两路相差信号-运行/停止字面量
unsigned char a4_s4[]="RUN", a4_s5[]="BACK";
// 状态4-两路相差信号-频率示数（s6~8，三位）
unsigned char a4_s6[]="1", a4_s7[]="0", a4_s8[]="0";
// 状态4-两路相差信号-相位示数（s9可变指针，8bit长, 9bit内存）
unsigned char *a4_s9;
// 状态4-两路相差信号-ADC入口
unsigned char a4_s10[]="ADC";

// 状态机5 -- 两路相差信号 -- 运行界面
unsigned char a5_s0[]="90D PHASE SIGNAL";
// 状态机5-两路相差信号-频率/相位字面量
unsigned char a5_s1[]="FREQ", a5_s2[]="KHz", a5_s3[]="PHASE";
// 状态机5-两路相差信号-运行/返回字面量
unsigned char a5_s4[]="RUNING", a5_s5[]="BACK";
// 状态机5-两路相差信号-频率示数（s6~8，三位）
unsigned char a5_s6[]="1", a5_s7[]="0", a5_s8[]="0";
// 状态机5-两路相差信号-相位示数占位，运行时由状态机4的相位字符串替换
unsigned char a5_s9[]="90.00  D";

// 状态机6 -- Costas锁相模式运行界面
unsigned char a6_s0[]="BPSK COSTAS";
// 状态机6-Costas锁相模式-ADC通道标签
unsigned char a6_s1[]="ADC1:", a6_s2[]="ADC2:";
// 状态机6-Costas锁相模式-锁定状态和返回字面量
unsigned char a6_s3[9]="LOCKING", a6_s4[]="BACK";
// 状态机6-Costas锁相模式-ADC数值显示缓冲区
unsigned char a6_s5[]="0000", a6_s6[]="0000";

// 状态机7 -- ADC查看界面
unsigned char a7_s0[]="ADC VALUE";
// 状态机7-ADC查看界面-ADC通道标签
unsigned char a7_s1[]="ADC1:", a7_s2[]="ADC2:";
// 状态机7-ADC查看界面-返回字面量
unsigned char a7_s3[]="BACK";
// 状态机7-ADC查看界面-ADC数值显示缓冲区
unsigned char a7_s4[]="0000", a7_s5[]="0000";

// 状态机8 -- 第二页模式选择
unsigned char a8_s0[]="CHOOSE MODE 2/2";
unsigned char a8_s1[]="BPSK COSTAS";
unsigned char a8_s2[]="QPSK DIAGONAL";
unsigned char a8_s3[]="QPSK AXIS 0/+-1";

// 状态机9 -- 频率校准运行界面
unsigned char a9_s0[]="FREQ CAL 100HZ";
unsigned char a9_s1[]="SPD:";
unsigned char a9_s2[]="REF:";
unsigned char a9_s3[9]="LOCKING";
unsigned char a9_s4[12]="125000000";
unsigned char a9_s5[12]="+000000";
unsigned char a9_s6[]="BACK";

// 状态机10 -- QPSK 锁相运行界面
unsigned char a10_s0[]="QPSK DIAGONAL";
unsigned char a10_s1[]="ADC1:", a10_s2[]="ADC2:";
unsigned char a10_s3[9]="LOCKING", a10_s4[]="BACK";
unsigned char a10_s5[]="0000", a10_s6[]="0000";

// 状态机11 -- 校准功能选择
unsigned char a11_s0[]="CALIBRATION";
unsigned char a11_s1[]="T POINT CAL";
unsigned char a11_s2[]="FREQ CAL";
unsigned char a11_s3[]="BACK";

// 状态机12 -- T 点原始值与测量范围
unsigned char a12_s0[17]="T CAL MEASURING";
unsigned char a12_s1[18]="I:0000 0000-0000";
unsigned char a12_s2[18]="Q:0000 0000-0000";
unsigned char a12_s3[]="PARAM";
unsigned char a12_s4[]="BACK";

// 状态机13 -- T 点标定参数
unsigned char a13_s0[17]="T CAL MID/AMP";
unsigned char a13_s1[18]="I M:0000 A:0000";
unsigned char a13_s2[18]="Q M:0000 A:0000";
unsigned char a13_s3[]="RESET";
unsigned char a13_s4[]="BACK";

// 状态机14 -- 轴向三态 QPSK 锁相运行界面
unsigned char a14_s0[]="QPSK AXIS 0/+-1";
unsigned char a14_s1[]="ADC1:", a14_s2[]="ADC2:";
unsigned char a14_s3[9]="LOCKING", a14_s4[]="BACK";
unsigned char a14_s5[]="0000", a14_s6[]="0000";

// 相位选择索引，范围 0..8，对应 string_degree 表
int degree_counter=8;
// 相位显示字符串表，索引值等于 AD9850 相位步进值，每步 11.25 度。
unsigned char string_degree[9][9] = {
	"  00.00D", "  11.25D", "  22.50D", "  33.75D", "  45.00D",
	"  56.25D", "  67.50D", "  78.75D", "  90.00D"
};



// ======================= 状态机字符串 ===========================

struct struct_act {
	unsigned char num;
	unsigned char *str[20];
	unsigned char x[20],y[20],inverse[20];
} a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14;

struct struct_act *act[15];

#define UI_INVERSE_NONE 0xff
// 可交互界面的反显字段下标表，行号对应状态机编号。
// 每一行按 DOWN 键轮换顺序登记；UP 键按相反方向轮换。
uint8_t ui_inverse_ptr[15][20] = {
    {0},                               // 状态机0：无交互
    {1, 2, 3},                         // 状态机1：模式选择
    {5, 6, 7, 8, 9, 10, 11, 12},       // 状态机2：模式1设置
    {6},                               // 状态机3：模式1运行
    {4, 10, 5, 6, 7, 8, 9},            // 状态机4：模式2设置
    {5},                               // 状态机5：模式2运行
    {4},                               // 状态机6：Costas运行
    {3},                               // 状态机7：ADC查看
    {1, 2, 3},                         // 状态机8：第二页模式选择
    {6},                               // 状态机9：校准运行
    {4},                               // 状态机10：QPSK运行
    {1, 2, 3},                         // 状态机11：校准功能选择
    {3, 4},                            // 状态机12：T点测量
    {3, 4},                            // 状态机13：T点参数
    {4}                                // 状态机14：轴向QPSK运行
};
uint8_t ui_inverse_count[15] = {0, 3, 8, 1, 7, 1, 1, 1, 3, 1, 1, 3, 2, 2, 1};

// 当前反显项在 ui_inverse_ptr 当前行中的位置，后续接入 UP/DOWN 时推进。
uint8_t ui_inverse_flag = 0;

// 状态机变量
unsigned int ui_state=0;

// UI状态编号说明
// 0：欢迎界面
// 1xx：模式选择界面
// 2xx：模式1，两路独立频率设置界面
// 3xx：模式1，两路独立频率运行界面
// 4xx：模式2，同频相位差设置界面
// 5xx：模式2，同频相位差运行界面
// 6xx：模式3，Costas闭环运行界面
// 7xx：ADC查看界面
// 8xx：第二页模式选择
// 9xx：频率校准运行界面
// 10xx：对角QPSK闭环运行界面
// 11xx：校准功能选择
// 12xx/13xx：T点测量与标定参数界面
// 14xx：轴向三态QPSK闭环运行界面


// 旧版 itodegree 已由 string_degree 表替代。

void itoafreq(unsigned int i, unsigned char* istr1,unsigned char* istr2,unsigned char* istr3) {
	unsigned int j;
	j=i/100;
	istr1[0]='0'+j;istr1[1]='\0';
	i=i-j*100;
	j=i/10;
	istr2[0]='0'+j;istr2[1]='\0';
	i=i-j*10;
	istr3[0]='0'+i;istr3[1]='\0';
}
unsigned int atoifreq(unsigned char* istr1,unsigned char* istr2,unsigned char* istr3) {
	unsigned int a,b,c,i;
	a=istr1[0]-'0';b=istr2[0]-'0';c=istr3[0]-'0';
	i = a*100+b*10+c;
	return i;
}
void in_de (unsigned int w, unsigned char* actstring ) {	//w=1 : increase; w=2 : decrease;
	if (w==1) {
		(*actstring)++;
		if (*actstring>'9') *actstring='0';
	}

	if (w==2) {
		(*actstring)--;
		if (*actstring<'0') *actstring='9';
	}

}
void in_de_degree(int degree_counter) {
	if (degree_counter < 0) degree_counter = 0;
	if (degree_counter > 8) degree_counter = 8;
	act[4]->str[9] = string_degree[degree_counter];
}




// 1 返回指定状态机登记的可反显字段数量。
// 2 act_index: 状态机编号，取值 0..14，对应 ui_inverse_ptr 的行号。
static uint8_t UI_GetInverseCount(uint8_t act_index);
// 1 将当前状态机的反显位置推进到下一个可反显字段，并重画旧字段和新字段。
// 2 act_index: 当前状态机编号，取值 1..13；状态机0没有可反显字段。
void UI_MoveInverseToNext(uint8_t act_index);
// 1 将当前状态机的反显位置推进到上一个可反显字段，并重画旧字段和新字段。
// 2 act_index: 当前状态机编号，取值 1..13；状态机0没有可反显字段。
void UI_MoveInverseToLast(uint8_t act_index);
// 显示状态机i
void display_ui_act(unsigned int i);
// 初始化状态机界面
void init_act(void);

// 1 在状态机切换时清除旧反显，设置新反显并刷新整个界面。
// 2 act_index: 目标状态机编号，取值 0..14；inverse_pos: ui_inverse_ptr 当前行中的位置，UI_INVERSE_NONE 表示不设置反显。
void UI_Transfer(uint8_t act_index, uint8_t inverse_pos);
static void UI_HandleEnter(void);
static void UI_HandleValueChange(uint8_t increase);
static void UI_UpdateRuntimeDisplay(void);

// UI本轮消费的按钮事件，由 Button_TakeFlags 在 ui_state_proc 入口刷新。
unsigned int key_ENTER_flag=0;
unsigned int key_DOWN_flag=0;
unsigned int key_UP_flag=0;
unsigned int key_INCREASE_flag=0;
unsigned int key_DECREASE_flag=0;

// 1 从按钮控制字中取出本轮 UI 要消费的事件。
static void UI_LoadButtonFlags(void);
uint8_t Button_Pressed(void);
void Button_Clear(void);



static void UI_UpdateRuntimeDisplay(void) {
    if (ui_state == 6) {
        strcpy((char *) a6_s3, Costas_IsLocked() ? "LOCKED" : "LOCKING");
        LCD_Print_String(a6.x[3], a6.y[3], a6.str[3], a6.inverse[3]);
        ADC_ToString(ADC0_Value[0], a6_s5);
        LCD_Print_String(a6.x[5], a6.y[5], a6.str[5], a6.inverse[5]);
        ADC_ToString(ADC0_Value[1], a6_s6);
        LCD_Print_String(a6.x[6], a6.y[6], a6.str[6], a6.inverse[6]);
    } else if (ui_state == 9) {
        strcpy((char *) a9_s3, Costas_CalibrationIsLocked() ? "LOCKED" : "LOCKING");
        sprintf((char *) a9_s4, "%09lu", (unsigned long) Costas_CalibrationGetReferenceHz());
        sprintf((char *) a9_s5, "%+07ld", (long) Costas_CalibrationGetSpeed());
        LCD_Print_String(a9.x[3], a9.y[3], a9.str[3], a9.inverse[3]);
        LCD_Print_String(a9.x[4], a9.y[4], a9.str[4], a9.inverse[4]);
        LCD_Print_String(a9.x[5], a9.y[5], a9.str[5], a9.inverse[5]);
    } else if (ui_state == 10) {
        strcpy((char *) a10_s3, Costas_IsLocked() ? "LOCKED" : "LOCKING");
        LCD_Print_String(a10.x[3], a10.y[3], a10.str[3], a10.inverse[3]);
        ADC_ToString(ADC0_Value[0], a10_s5);
        LCD_Print_String(a10.x[5], a10.y[5], a10.str[5], a10.inverse[5]);
        ADC_ToString(ADC0_Value[1], a10_s6);
        LCD_Print_String(a10.x[6], a10.y[6], a10.str[6], a10.inverse[6]);
    } else if (ui_state == 14) {
        strcpy((char *) a14_s3, Costas_IsLocked() ? "LOCKED" : "LOCKING");
        LCD_Print_String(a14.x[3], a14.y[3], a14.str[3], a14.inverse[3]);
        ADC_ToString(ADC0_Value[0], a14_s5);
        LCD_Print_String(a14.x[5], a14.y[5], a14.str[5], a14.inverse[5]);
        ADC_ToString(ADC0_Value[1], a14_s6);
        LCD_Print_String(a14.x[6], a14.y[6], a14.str[6], a14.inverse[6]);
    } else if (ui_state == 12) {
        strcpy((char *) a12_s0, Costas_TPointCalibrationIsValid() ? "T CAL VALID" : "T CAL MEASURING");
        sprintf((char *) a12_s1, "I:%04lu %04lu-%04lu", (unsigned long) ADC0_Value[0],
            (unsigned long) Costas_TPointGetMin(0), (unsigned long) Costas_TPointGetMax(0));
        sprintf((char *) a12_s2, "Q:%04lu %04lu-%04lu", (unsigned long) ADC0_Value[1],
            (unsigned long) Costas_TPointGetMin(1), (unsigned long) Costas_TPointGetMax(1));
        LCD_Print_String(a12.x[0], a12.y[0], a12.str[0], a12.inverse[0]);
        LCD_Print_String(a12.x[1], a12.y[1], a12.str[1], a12.inverse[1]);
        LCD_Print_String(a12.x[2], a12.y[2], a12.str[2], a12.inverse[2]);
    } else if (ui_state == 13) {
        sprintf((char *) a13_s1, "I M:%04ld A:%04ld", (long) Costas_TPointGetMid(0),
            (long) Costas_TPointGetAmp(0));
        sprintf((char *) a13_s2, "Q M:%04ld A:%04ld", (long) Costas_TPointGetMid(1),
            (long) Costas_TPointGetAmp(1));
        LCD_Print_String(a13.x[1], a13.y[1], a13.str[1], a13.inverse[1]);
        LCD_Print_String(a13.x[2], a13.y[2], a13.str[2], a13.inverse[2]);
    } else if (ui_state == 7) {
        ADC_ToString(ADC0_Avg_Value[0], a7_s4);
        LCD_Print_String(a7.x[4], a7.y[4], a7.str[4], a7.inverse[4]);
        ADC_ToString(ADC0_Avg_Value[1], a7_s5);
        LCD_Print_String(a7.x[5], a7.y[5], a7.str[5], a7.inverse[5]);
    }
}

// 1 按低速节拍刷新运行界面的 ADC、锁定状态和校准调试量，避免 LCD 写入阻塞高速闭环。
void UI_RuntimeDisplayTask(void) {
    UI_UpdateRuntimeDisplay();
}

static void UI_HandleValueChange(uint8_t increase) {
    uint8_t item;

    if (ui_state == 2 && ui_inverse_flag >= 2) {
        item = ui_inverse_ptr[2][ui_inverse_flag];
        in_de(increase ? 1 : 2, act[2]->str[item]);
        LCD_Print_String(act[2]->x[item], act[2]->y[item], act[2]->str[item], act[2]->inverse[item]);
    } else if (ui_state == 4) {
        item = ui_inverse_ptr[4][ui_inverse_flag];
        if (ui_inverse_flag >= 3 && ui_inverse_flag <= 5) {
            in_de(increase ? 1 : 2, act[4]->str[item]);
            LCD_Print_String(act[4]->x[item], act[4]->y[item], act[4]->str[item], act[4]->inverse[item]);
        } else if (ui_inverse_flag == 6) {
            if (increase) {
                degree_counter++;
                if (degree_counter > 8) degree_counter = 0;
            } else {
                degree_counter--;
                if (degree_counter < 0) degree_counter = 8;
            }
            in_de_degree(degree_counter);
            LCD_Print_String(act[4]->x[9], act[4]->y[9], act[4]->str[9], act[4]->inverse[9]);
        }
    }
}

static void UI_HandleEnter(void) {
    switch (ui_state) {
        case 0:
            UI_Transfer(1, 0);
            break;

        case 1:
            if (ui_inverse_flag == 0) {
                UI_Transfer(2, 2);
            } else if (ui_inverse_flag == 1) {
                a4_s9 = string_degree[mode2_phase_diff];
                a4.str[9] = a4_s9;
                UI_Transfer(4, 3);
            } else if (ui_inverse_flag == 2) {
                UI_Transfer(11, 0);
            }
            break;

        case 2:
            if (ui_inverse_flag == 0) {
                act[3]->str[7] = act[2]->str[7];
                act[3]->str[8] = act[2]->str[8];
                act[3]->str[9] = act[2]->str[9];
                act[3]->str[10] = act[2]->str[10];
                act[3]->str[11] = act[2]->str[11];
                act[3]->str[12] = act[2]->str[12];
                AppState_StartMode1(atoifreq(act[2]->str[7], act[2]->str[8], act[2]->str[9]),
                    atoifreq(act[2]->str[10], act[2]->str[11], act[2]->str[12]));
                UI_Transfer(3, 0);
            } else if (ui_inverse_flag == 1) {
                itoafreq(mode1_signal1_freq, a2_s7, a2_s8, a2_s9);
                itoafreq(mode1_signal2_freq, a2_s10, a2_s11, a2_s12);
                AppState_Stop();
                UI_Transfer(1, 0);
            }
            break;

        case 3:
            AppState_SelectMode(1);
            UI_Transfer(2, 0);
            break;

        case 4:
            if (ui_inverse_flag == 0) {
                act[5]->str[6] = act[4]->str[6];
                act[5]->str[7] = act[4]->str[7];
                act[5]->str[8] = act[4]->str[8];
                act[5]->str[9] = act[4]->str[9];
                AppState_StartMode2(atoifreq(act[4]->str[6], act[4]->str[7], act[4]->str[8]), degree_counter);
                UI_Transfer(5, 0);
            } else if (ui_inverse_flag == 1) {
                itoafreq(mode2_freq, a4_s6, a4_s7, a4_s8);
                a4_s9 = string_degree[mode2_phase_diff];
                a4.str[9] = a4_s9;
                AppState_SelectMode(2);
                UI_Transfer(7, 0);
            } else if (ui_inverse_flag == 2) {
                itoafreq(mode2_freq, a4_s6, a4_s7, a4_s8);
                a4_s9 = string_degree[mode2_phase_diff];
                a4.str[9] = a4_s9;
                AppState_Stop();
                UI_Transfer(1, 1);
            }
            break;

        case 5:
            AppState_SelectMode(2);
            UI_Transfer(4, 0);
            break;

        case 6:
            AppState_Stop();
            UI_Transfer(8, 0);
            break;

        case 7:
            AppState_SelectMode(2);
            UI_Transfer(4, 1);
            break;

        // 第二页模式选择：BPSK、对角 QPSK 和轴向三态 QPSK 分别使用独立业务状态。
        case 8:
            if (ui_inverse_flag == 0) {
                AppState_StartBpsk();
                UI_Transfer(6, 0);
            } else if (ui_inverse_flag == 1) {
                AppState_StartQpskDiagonal();
                UI_Transfer(10, 0);
            } else {
                AppState_StartQpskAxis();
                UI_Transfer(14, 0);
            }
            break;

        // 校准运行页返回第一页校准选项。
        case 9:
            AppState_Stop();
            UI_Transfer(11, 1);
            break;

        // QPSK 运行页返回第二页 QPSK 选项。
        case 10:
            AppState_Stop();
            UI_Transfer(8, 1);
            break;

        // 校准子菜单：T 点 ADC/IQ 标定、频率校准或返回主菜单。
        case 11:
            if (ui_inverse_flag == 0) {
                AppState_StartTPointCalibration();
                UI_Transfer(12, 0);
            } else if (ui_inverse_flag == 1) {
                AppState_StartCalibration();
                UI_Transfer(9, 0);
            } else {
                AppState_Stop();
                UI_Transfer(1, 2);
            }
            break;

        // T 点测量页可查看推导参数或返回校准子菜单。
        case 12:
            if (ui_inverse_flag == 0) {
                UI_Transfer(13, 0);
            } else {
                AppState_Stop();
                UI_Transfer(11, 0);
            }
            break;

        // 参数页 RESET 会清空极值并开始新一轮测量，BACK 返回测量页。
        case 13:
            if (ui_inverse_flag == 0) {
                AppState_StartTPointCalibration();
                UI_Transfer(12, 0);
            } else {
                UI_Transfer(12, 0);
            }
            break;

        // 轴向三态 QPSK 运行页返回第二页对应选项。
        case 14:
            AppState_Stop();
            UI_Transfer(8, 2);
            break;

        default:
            UI_Transfer(0, UI_INVERSE_NONE);
            break;
    }
}

void ui_state_proc(void) // 根据当前 UI 状态处理一轮按钮事件
{
    UI_LoadButtonFlags();

    if (ui_state == 0 && Button_Pressed()) {
        UI_Transfer(1, 0);
        Button_Clear();
        return;
    }

    // 模式选择页首尾不在本页循环，而是连续滚动到相邻页面。
    if (key_UP_flag && ui_state == 1 && ui_inverse_flag == 0) {
        UI_Transfer(8, 1);
    } else if (key_UP_flag && ui_state == 8 && ui_inverse_flag == 0) {
        UI_Transfer(1, 2);
    } else if (key_DOWN_flag && ui_state == 1 && ui_inverse_flag == 2) {
        UI_Transfer(8, 0);
    } else if (key_DOWN_flag && ui_state == 8 && ui_inverse_flag == 2) {
        UI_Transfer(1, 0);
    } else if (key_UP_flag) {
        UI_MoveInverseToLast(ui_state);
        display_ui_act(ui_state);
    } else if (key_DOWN_flag) {
        UI_MoveInverseToNext(ui_state);
        display_ui_act(ui_state);
    } else if (key_INCREASE_flag) {
        UI_HandleValueChange(1);
    } else if (key_DECREASE_flag) {
        UI_HandleValueChange(0);
    } else if (key_ENTER_flag) {
        UI_HandleEnter();
    }

    Button_Clear();
}

static uint8_t UI_GetInverseCount(uint8_t act_index) {
	return act_index >= 15 ? 0 : ui_inverse_count[act_index];
}
void UI_MoveInverseToNext(uint8_t act_index) {
	uint8_t count;
	uint8_t old_item;
	uint8_t new_item;

	// 越界保护
	count = UI_GetInverseCount(act_index);
	if (count == 0) return;
	if (ui_inverse_flag >= count) {
		ui_inverse_flag = 0;
	}

	old_item = ui_inverse_ptr[act_index][ui_inverse_flag];
	ui_inverse_flag = (ui_inverse_flag + 1) % count;
	new_item = ui_inverse_ptr[act_index][ui_inverse_flag];

	act[act_index]->inverse[old_item] = 0;
	act[act_index]->inverse[new_item] = 1;
}
void UI_MoveInverseToLast(uint8_t act_index) {
	uint8_t count;
	uint8_t old_item;
	uint8_t new_item;

	// 越界保护
	count = UI_GetInverseCount(act_index);
	if (count == 0) return;
	if (ui_inverse_flag >= count) {
		ui_inverse_flag = 0;
	}

	old_item = ui_inverse_ptr[act_index][ui_inverse_flag];
	ui_inverse_flag = (ui_inverse_flag + count - 1) % count;
	new_item = ui_inverse_ptr[act_index][ui_inverse_flag];

	act[act_index]->inverse[old_item] = 0;
	act[act_index]->inverse[new_item] = 1;
}
void display_ui_act(unsigned int i) {
	unsigned int j = 0;
	LCD_Clear_Panel();
	for (j = 0; j < act[i]->num; j++) {
		LCD_Print_String(act[i]->x[j], act[i]->y[j], act[i]->str[j], act[i]->inverse[j]);
	}
}
void init_act(void) {

    itoafreq(mode2_freq, a4_s6, a4_s7, a4_s8);
    a4_s9 = string_degree[mode2_phase_diff];

    /* 状态机界面说明
     * N(x,y) 代表第 N 号字符串显示在文本格坐标 (x,y)。
     * N[x,y] 代表第 N 号字符串是可变显示字段，坐标仍为 (x,y)。
     * inverse=1 表示当前光标/选中项反显。
     */

    // 状态机0 -- 欢迎界面
    // 0(0,0) Costas Loop
    // 1(1,0) WELCOME
    // 2(2,0) PRESS ANY KEY
    a0.num=3;
    a0.str[0]=a0_s0; a0.x[0]=0;  a0.y[0]=0;  a0.inverse[0]=0;
    a0.str[1]=a0_s1; a0.x[1]=1;  a0.y[1]=0;  a0.inverse[1]=0;
    a0.str[2]=a0_s2; a0.x[2]=2;  a0.y[2]=0;  a0.inverse[2]=0;
    act[0]=&a0;

    // 状态机1 -- 选择模式
    // 0(0,0) CHOOSE OP MODE
    // 1(1,0) DUO INDEP SIGNAL，对应模式1：双路独立频率
    // 2(2,0) 90D PHASE SIGNAL，对应模式2：同频可调相位
    // 3(3,0) CALIBRATION；继续向下进入第二页锁相模式
    a1.num=4;
    a1.str[0]=a1_s0;    a1.x[0]=0;    a1.y[0]=0;    a1.inverse[0]=0;
    a1.str[1]=a1_s1;    a1.x[1]=1;    a1.y[1]=0;    a1.inverse[1]=0;
    a1.str[2]=a1_s2;    a1.x[2]=2;    a1.y[2]=0;    a1.inverse[2]=0;
    a1.str[3]=a1_s3;    a1.x[3]=3;    a1.y[3]=0;    a1.inverse[3]=0;
    act[1]=&a1;

    // 状态机2 -- 两路独立信号调试界面
    // 0(0,0) DUO INDEP SIGNAL
    // 1(1,0) FREQ 1，7~9[1,5~7] 第一路频率三位数字，2(1,8) KHz
    // 3(2,0) FREQ 2，10~12[2,5~7] 第二路频率三位数字，4(2,8) KHz
    // 5(3,0) RUN，6(3,12) BACK
    a2.num=13;
    a2.str[0]=a2_s0;    a2.x[0]=0;    a2.y[0]=0;    a2.inverse[0]=0;
    a2.str[1]=a2_s1;    a2.x[1]=1;    a2.y[1]=0;    a2.inverse[1]=0;
    a2.str[2]=a2_s2;    a2.x[2]=1;    a2.y[2]=8;    a2.inverse[2]=0;
    a2.str[3]=a2_s3;    a2.x[3]=2;    a2.y[3]=0;    a2.inverse[3]=0;
    a2.str[4]=a2_s4;    a2.x[4]=2;    a2.y[4]=8;    a2.inverse[4]=0;
    a2.str[5]=a2_s5;    a2.x[5]=3;    a2.y[5]=0;    a2.inverse[5]=0;
    a2.str[6]=a2_s6;    a2.x[6]=3;    a2.y[6]=12;   a2.inverse[6]=0;
    a2.str[7]=a2_s7;    a2.x[7]=1;    a2.y[7]=5;    a2.inverse[7]=0;
    a2.str[8]=a2_s8;    a2.x[8]=1;    a2.y[8]=6;    a2.inverse[8]=0;
    a2.str[9]=a2_s9;    a2.x[9]=1;    a2.y[9]=7;    a2.inverse[9]=0;
    a2.str[10]=a2_s10;  a2.x[10]=2;   a2.y[10]=5;   a2.inverse[10]=0;
    a2.str[11]=a2_s11;  a2.x[11]=2;   a2.y[11]=6;   a2.inverse[11]=0;
    a2.str[12]=a2_s12;  a2.x[12]=2;   a2.y[12]=7;   a2.inverse[12]=0;
    act[2]=&a2;

    // 状态机3 -- 两路独立信号运行界面
    // 布局与状态机2一致，但 5(3,0) 显示 RUNNING，6(3,12) 为 BACK。
    // 7~12 显示启动时复制过来的两路频率。
    a3.num=13;
    a3.str[0]=a3_s0;    a3.x[0]=0;    a3.y[0]=0;    a3.inverse[0]=0;
    a3.str[1]=a3_s1;    a3.x[1]=1;    a3.y[1]=0;    a3.inverse[1]=0;
    a3.str[2]=a3_s2;    a3.x[2]=1;    a3.y[2]=8;    a3.inverse[2]=0;
    a3.str[3]=a3_s3;    a3.x[3]=2;    a3.y[3]=0;    a3.inverse[3]=0;
    a3.str[4]=a3_s4;    a3.x[4]=2;    a3.y[4]=8;    a3.inverse[4]=0;
    a3.str[5]=a3_s5;    a3.x[5]=3;    a3.y[5]=0;    a3.inverse[5]=0;
    a3.str[6]=a3_s6;    a3.x[6]=3;    a3.y[6]=12;   a3.inverse[6]=0;
    a3.str[7]=a3_s7;    a3.x[7]=1;    a3.y[7]=5;    a3.inverse[7]=0;
    a3.str[8]=a3_s8;    a3.x[8]=1;    a3.y[8]=6;    a3.inverse[8]=0;
    a3.str[9]=a3_s9;    a3.x[9]=1;    a3.y[9]=7;    a3.inverse[9]=0;
    a3.str[10]=a3_s10;  a3.x[10]=2;   a3.y[10]=5;   a3.inverse[10]=0;
    a3.str[11]=a3_s11;  a3.x[11]=2;   a3.y[11]=6;   a3.inverse[11]=0;
    a3.str[12]=a3_s12;  a3.x[12]=2;   a3.y[12]=7;   a3.inverse[12]=0;
    act[3]=&a3;

    // 状态机4 -- 两路相差信号调试界面
    // 0(0,0) 90D PHASE SIGNAL
    // 1(1,0) FREQ，6~8[1,5~7] 频率三位数字，2(1,8) KHz
    // 3(2,0) PHASE，9[2,5] 相位显示字符串
    // 4(3,0) RUN，10(3,6) ADC入口，5(3,12) BACK
    a4.num=11;
    a4.str[0]=a4_s0;    a4.x[0]=0;    a4.y[0]=0;    a4.inverse[0]=0;
    a4.str[1]=a4_s1;    a4.x[1]=1;    a4.y[1]=0;    a4.inverse[1]=0;
    a4.str[2]=a4_s2;    a4.x[2]=1;    a4.y[2]=8;    a4.inverse[2]=0;
    a4.str[3]=a4_s3;    a4.x[3]=2;    a4.y[3]=0;    a4.inverse[3]=0;
    a4.str[4]=a4_s4;    a4.x[4]=3;    a4.y[4]=0;    a4.inverse[4]=0;
    a4.str[5]=a4_s5;    a4.x[5]=3;    a4.y[5]=12;   a4.inverse[5]=0;
    a4.str[6]=a4_s6;    a4.x[6]=1;    a4.y[6]=5;    a4.inverse[6]=0;
    a4.str[7]=a4_s7;    a4.x[7]=1;    a4.y[7]=6;    a4.inverse[7]=0;
    a4.str[8]=a4_s8;    a4.x[8]=1;    a4.y[8]=7;    a4.inverse[8]=0;
    a4.str[9]=a4_s9;    a4.x[9]=2;    a4.y[9]=5;    a4.inverse[9]=0;
    a4.str[10]=a4_s10;  a4.x[10]=3;   a4.y[10]=6;   a4.inverse[10]=0;
    act[4]=&a4;

    // 状态机5 -- 两路相差信号运行界面
    // 布局与状态机4一致，但 4(3,0) 显示 RUNING，5(3,12) 为 BACK。
    // 6~9 显示启动时复制过来的频率和相位。
    a5.num=10;
    a5.str[0]=a5_s0;    a5.x[0]=0;    a5.y[0]=0;    a5.inverse[0]=0;
    a5.str[1]=a5_s1;    a5.x[1]=1;    a5.y[1]=0;    a5.inverse[1]=0;
    a5.str[2]=a5_s2;    a5.x[2]=1;    a5.y[2]=8;    a5.inverse[2]=0;
    a5.str[3]=a5_s3;    a5.x[3]=2;    a5.y[3]=0;    a5.inverse[3]=0;
    a5.str[4]=a5_s4;    a5.x[4]=3;    a5.y[4]=0;    a5.inverse[4]=0;
    a5.str[5]=a5_s5;    a5.x[5]=3;    a5.y[5]=12;   a5.inverse[5]=0;
    a5.str[6]=a5_s6;    a5.x[6]=1;    a5.y[6]=5;    a5.inverse[6]=0;
    a5.str[7]=a5_s7;    a5.x[7]=1;    a5.y[7]=6;    a5.inverse[7]=0;
    a5.str[8]=a5_s8;    a5.x[8]=1;    a5.y[8]=7;    a5.inverse[8]=0;
    a5.str[9]=a5_s9;    a5.x[9]=2;    a5.y[9]=5;    a5.inverse[9]=0;
    act[5]=&a5;

    // 状态机6 -- Costas锁相运行界面
    // 0(0,0) COSTAS LOOP
    // 1(1,0) ADC1:，5[1,5] ADC1数值
    // 2(2,0) ADC2:，6[2,5] ADC2数值
    // 3(3,0) LOCKING/LOCKED 状态文字，4(3,12) BACK
    a6.num=7;
    a6.str[0]=a6_s0;    a6.x[0]=0;    a6.y[0]=0;    a6.inverse[0]=0;
    a6.str[1]=a6_s1;    a6.x[1]=1;    a6.y[1]=0;    a6.inverse[1]=0;
    a6.str[2]=a6_s2;    a6.x[2]=2;    a6.y[2]=0;    a6.inverse[2]=0;
    a6.str[3]=a6_s3;    a6.x[3]=3;    a6.y[3]=0;    a6.inverse[3]=0;
    a6.str[4]=a6_s4;    a6.x[4]=3;    a6.y[4]=12;   a6.inverse[4]=0;
    a6.str[5]=a6_s5;    a6.x[5]=1;    a6.y[5]=5;    a6.inverse[5]=0;
    a6.str[6]=a6_s6;    a6.x[6]=2;    a6.y[6]=5;    a6.inverse[6]=0;
    act[6]=&a6;

    // 状态机7 -- ADC查看界面
    // 0(0,0) ADC VALUE
    // 1(1,0) ADC1:，4[1,5] ADC1数值
    // 2(2,0) ADC2:，5[2,5] ADC2数值
    // 3(3,12) BACK
    a7.num=6;
    a7.str[0]=a7_s0;    a7.x[0]=0;    a7.y[0]=0;    a7.inverse[0]=0;
    a7.str[1]=a7_s1;    a7.x[1]=1;    a7.y[1]=0;    a7.inverse[1]=0;
    a7.str[2]=a7_s2;    a7.x[2]=2;    a7.y[2]=0;    a7.inverse[2]=0;
    a7.str[3]=a7_s3;    a7.x[3]=3;    a7.y[3]=12;   a7.inverse[3]=0;
    a7.str[4]=a7_s4;    a7.x[4]=1;    a7.y[4]=5;    a7.inverse[4]=0;
    a7.str[5]=a7_s5;    a7.x[5]=2;    a7.y[5]=5;    a7.inverse[5]=0;
    act[7]=&a7;

    // 状态机8 -- 第二页模式选择
    a8.num=4;
    a8.str[0]=a8_s0; a8.x[0]=0; a8.y[0]=0; a8.inverse[0]=0;
    a8.str[1]=a8_s1; a8.x[1]=1; a8.y[1]=0; a8.inverse[1]=0;
    a8.str[2]=a8_s2; a8.x[2]=2; a8.y[2]=0; a8.inverse[2]=0;
    a8.str[3]=a8_s3; a8.x[3]=3; a8.y[3]=0; a8.inverse[3]=0;
    act[8]=&a8;

    // 状态机9 -- 外部100kHz参考/本地约99.9kHz小拍频校准
    a9.num=7;
    a9.str[0]=a9_s0; a9.x[0]=0; a9.y[0]=0;  a9.inverse[0]=0;
    a9.str[1]=a9_s1; a9.x[1]=1; a9.y[1]=0;  a9.inverse[1]=0;
    a9.str[2]=a9_s2; a9.x[2]=2; a9.y[2]=0;  a9.inverse[2]=0;
    a9.str[3]=a9_s3; a9.x[3]=3; a9.y[3]=0;  a9.inverse[3]=0;
    a9.str[4]=a9_s4; a9.x[4]=2; a9.y[4]=5;  a9.inverse[4]=0;
    a9.str[5]=a9_s5; a9.x[5]=1; a9.y[5]=5;  a9.inverse[5]=0;
    a9.str[6]=a9_s6; a9.x[6]=3; a9.y[6]=12; a9.inverse[6]=0;
    act[9]=&a9;

    // 状态机10 -- 对角 QPSK 锁相运行
    a10.num=7;
    a10.str[0]=a10_s0; a10.x[0]=0; a10.y[0]=0;  a10.inverse[0]=0;
    a10.str[1]=a10_s1; a10.x[1]=1; a10.y[1]=0;  a10.inverse[1]=0;
    a10.str[2]=a10_s2; a10.x[2]=2; a10.y[2]=0;  a10.inverse[2]=0;
    a10.str[3]=a10_s3; a10.x[3]=3; a10.y[3]=0;  a10.inverse[3]=0;
    a10.str[4]=a10_s4; a10.x[4]=3; a10.y[4]=12; a10.inverse[4]=0;
    a10.str[5]=a10_s5; a10.x[5]=1; a10.y[5]=5;  a10.inverse[5]=0;
    a10.str[6]=a10_s6; a10.x[6]=2; a10.y[6]=5;  a10.inverse[6]=0;
    act[10]=&a10;

    // 状态机11 -- 校准功能选择
    a11.num=4;
    a11.str[0]=a11_s0; a11.x[0]=0; a11.y[0]=0;  a11.inverse[0]=0;
    a11.str[1]=a11_s1; a11.x[1]=1; a11.y[1]=0;  a11.inverse[1]=0;
    a11.str[2]=a11_s2; a11.x[2]=2; a11.y[2]=0;  a11.inverse[2]=0;
    a11.str[3]=a11_s3; a11.x[3]=3; a11.y[3]=12; a11.inverse[3]=0;
    act[11]=&a11;

    // 状态机12 -- T 点当前 ADC 值和本轮 MIN/MAX
    a12.num=5;
    a12.str[0]=a12_s0; a12.x[0]=0; a12.y[0]=0;  a12.inverse[0]=0;
    a12.str[1]=a12_s1; a12.x[1]=1; a12.y[1]=0;  a12.inverse[1]=0;
    a12.str[2]=a12_s2; a12.x[2]=2; a12.y[2]=0;  a12.inverse[2]=0;
    a12.str[3]=a12_s3; a12.x[3]=3; a12.y[3]=0;  a12.inverse[3]=0;
    a12.str[4]=a12_s4; a12.x[4]=3; a12.y[4]=12; a12.inverse[4]=0;
    act[12]=&a12;

    // 状态机13 -- T 点推导出的 I/Q MID 和 AMP
    a13.num=5;
    a13.str[0]=a13_s0; a13.x[0]=0; a13.y[0]=0;  a13.inverse[0]=0;
    a13.str[1]=a13_s1; a13.x[1]=1; a13.y[1]=0;  a13.inverse[1]=0;
    a13.str[2]=a13_s2; a13.x[2]=2; a13.y[2]=0;  a13.inverse[2]=0;
    a13.str[3]=a13_s3; a13.x[3]=3; a13.y[3]=0;  a13.inverse[3]=0;
    a13.str[4]=a13_s4; a13.x[4]=3; a13.y[4]=12; a13.inverse[4]=0;
    act[13]=&a13;

    // 状态机14 -- 轴向三态 QPSK 锁相运行
    a14.num=7;
    a14.str[0]=a14_s0; a14.x[0]=0; a14.y[0]=0;  a14.inverse[0]=0;
    a14.str[1]=a14_s1; a14.x[1]=1; a14.y[1]=0;  a14.inverse[1]=0;
    a14.str[2]=a14_s2; a14.x[2]=2; a14.y[2]=0;  a14.inverse[2]=0;
    a14.str[3]=a14_s3; a14.x[3]=3; a14.y[3]=0;  a14.inverse[3]=0;
    a14.str[4]=a14_s4; a14.x[4]=3; a14.y[4]=12; a14.inverse[4]=0;
    a14.str[5]=a14_s5; a14.x[5]=1; a14.y[5]=5;  a14.inverse[5]=0;
    a14.str[6]=a14_s6; a14.x[6]=2; a14.y[6]=5;  a14.inverse[6]=0;
    act[14]=&a14;

    UI_Transfer(0, UI_INVERSE_NONE);
}
void UI_Transfer(uint8_t act_index, uint8_t inverse_pos){
	uint8_t i;

	// 状态机越界保护
	if (act_index >= 15) return;

	// 清空原状态机反色
	for (i = 0; i < act[ui_state]->num; i++) act[ui_state]->inverse[i] = 0;

	// 准备布置新状态机反色
	ui_state = act_index;
	for (i = 0; i < act[ui_state]->num; i++) act[ui_state]->inverse[i] = 0;
	// 排除 无反色或越界 的情况
	if (inverse_pos != UI_INVERSE_NONE && inverse_pos < ui_inverse_count[act_index]) {
		ui_inverse_flag = inverse_pos;
		act[ui_state]->inverse[ ui_inverse_ptr[ui_state][inverse_pos] ] = 1;
	} else {
		ui_inverse_flag = 0;
	}

	display_ui_act(ui_state);
}
static void UI_LoadButtonFlags(void) {
	uint8_t button_flags = Button_TakeFlags();

	key_ENTER_flag = (button_flags & BUTTON_FLAG_ENTER) != 0;
	key_UP_flag = (button_flags & BUTTON_FLAG_UP) != 0;
	key_DOWN_flag = (button_flags & BUTTON_FLAG_DOWN) != 0;
	key_INCREASE_flag = (button_flags & BUTTON_FLAG_INCREASE) != 0;
	key_DECREASE_flag = (button_flags & BUTTON_FLAG_DECREASE) != 0;
}

uint8_t Button_Pressed(void) {
	return key_ENTER_flag||key_UP_flag||key_DOWN_flag||key_INCREASE_flag||key_DECREASE_flag;
}
void Button_Clear(void) {
	key_ENTER_flag = key_UP_flag = key_DOWN_flag = key_INCREASE_flag = key_DECREASE_flag = 0;
}








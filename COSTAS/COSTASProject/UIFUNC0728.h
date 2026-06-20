#ifndef UIFUNC0728_H
#define UIFUNC0728_H

#include <stdint.h>

#define BUTTON_FLAG_ENTER     0x01
#define BUTTON_FLAG_UP        0x02
#define BUTTON_FLAG_DOWN      0x04
#define BUTTON_FLAG_INCREASE  0x08
#define BUTTON_FLAG_DECREASE  0x10

extern unsigned int ui_state;

void Button_Task(void);
uint8_t Button_TakeFlags(void);

void init_act(void);
void ui_state_proc(void);
// 由主循环低速节拍调用，刷新运行状态而不阻塞闭环采样。
void UI_RuntimeDisplayTask(void);

#endif

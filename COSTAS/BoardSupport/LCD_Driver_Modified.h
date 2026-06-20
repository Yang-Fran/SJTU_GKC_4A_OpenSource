#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

// 板级 SPI LCD 驱动的公开接口。
// 注意，这一版相比库驱动的LCD_Driver，改进了显示的功能性
// 更新时间：2026.05 氧化物

#include <stdint.h>

void LCD_Init(void);
void WriteData(unsigned char data);
void SetPACA(unsigned char PA, unsigned char CA);
void Disable_Buzzer(void);
void Draw_Word_8x16(unsigned char x, unsigned char y, unsigned char ascii_word, unsigned char inverse);
void LCD_Print_String(unsigned char line, unsigned char column, unsigned char *str, unsigned char inverse);
void LCD_Clear_Panel(void);

#endif

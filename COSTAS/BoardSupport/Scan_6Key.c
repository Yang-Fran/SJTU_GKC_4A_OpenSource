#include <stdio.h>
#include "DrvGPIO.h"

// 板级 3x2 键盘扫描驱动，适配本项目实际使用的按键矩阵。
// 比较特殊的点在于，最下面一排的三个按钮由于接线问题，是不能用的，所以不使用。
// GPIO 用途：GPA0..GPA2 输出扫描行模式，GPA3..GPA4 读取两列按键输入。
// 状态变量：无，ScanKey 每次直接返回当前扫描到的按键值。
// 更新时间：2026.05 氧化物

// 1 切换键盘扫描输出后做短暂稳定延时。
void delay(void) {
    int j;
    for (j = 0; j < 10; j++);
}

// 1 将 GPA0..GPA4 配置为准双向模式，用于键盘矩阵扫描。
// 这里的准双向是工作在"High"(弱高电平/上拉) 和 "0"(强低电平/直接接地)
// 所以可以通过给 High - 按钮 - 0 的形式上电，然后检测High是否被拉低，判断按钮状态
void OpenKeyPad(void) {
    uint8_t i;
    /* Initial key pad */
    for (i = 0; i < 5; i++)
        DrvGPIO_Open(E_GPA, i, E_IO_QUASI);
}

// 1 关闭 GPA0..GPA4 键盘 GPIO 引脚。
void CloseKeyPad(void) {
    uint8_t i;

    for (i = 0; i < 5; i++)
        DrvGPIO_Close(E_GPA, i);
}

// 1 扫描键盘矩阵并返回按键编号。
// 2 返回值: 0 表示无按键，1..6 表示扫描到的行列位置。
//----------------------------------------------
//                PA0    PA1     PA2
//                 |      |       |
// 输入 PA3 ───── 按键1 - 按键2 - 按键3
// 输入 PA4 ───── 按键4 - 按键5 - 按键6
uint8_t ScanKey(void) {
    uint8_t act[4] = {0x3b, 0x3d, 0x3e};
    uint8_t i, temp, pin;

    // 逐行扫描
    for (i = 0; i < 3; i++) {

        // 对 PA 0 ~ 4 依次置位，PA3,4都是High，PA012依次是低
        temp = act[i];
        for (pin = 0; pin < 5; pin++) {
            if ((temp & 0x01) == 0x01)
                DrvGPIO_SetBit(E_GPA, pin);
            else
                DrvGPIO_ClrBit(E_GPA, pin);
            temp >>= 1;
        }

        // 留一个小延迟
        delay();

        // 读 PA3/4 上的数据
        if (DrvGPIO_GetBit(E_GPA, 3) == 0)
            return (i + 1);
        if (DrvGPIO_GetBit(E_GPA, 4) == 0)
            return (i + 4);
    }
    // 未识别到按键，返回0
    return 0;
}

#ifndef SCAN_6KEY_H
#define SCAN_6KEY_H

// 板级 3x2 键盘扫描驱动的公开接口。
// 更新时间：2026.05 氧化物

#include <stdint.h>

void OpenKeyPad(void);
void CloseKeyPad(void);
uint8_t ScanKey(void);

#endif

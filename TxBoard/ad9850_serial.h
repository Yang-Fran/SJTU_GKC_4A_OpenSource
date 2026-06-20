#ifndef __BSP_AD9850_H
#define __BSP_AD9850_H

#include <stdint.h>

/* 函数声明 */
void AD9850_InitHard(void);
void AD9850_WriteCmd(uint8_t _ucPhase, double _dOutFreq);
void AD98850_Output_0(void);

#endif /* __BSP_AD9850_H */

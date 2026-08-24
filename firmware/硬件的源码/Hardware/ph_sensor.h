#ifndef _PH_SENSOR_H_
#define _PH_SENSOR_H_

#include "stm32f10x.h"

/* pH传感器 (模拟量) PA1 - ADC1_CH1 (共用ADC) */

void PH_Init(void);
float PH_Read(void);        /* 返回pH值 0~14 */

#endif

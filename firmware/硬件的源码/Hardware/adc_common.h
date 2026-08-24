#ifndef _ADC_COMMON_H_
#define _ADC_COMMON_H_

#include "stm32f10x.h"

/**
 * ADC通用模块 — ADC1双通道采集
 * CH0: PA0 — MQ2空气质量传感器
 * CH1: PA1 — pH传感器 / 备用ADC输入
 *
 * 两个传感器共用ADC1, 分时切换通道读取
 */

void ADC_Common_Init(void);
uint16_t ADC_Read_CH0(void);   /* PA0 - MQ2 */
uint16_t ADC_Read_CH1(void);   /* PA1 - pH  */

#endif

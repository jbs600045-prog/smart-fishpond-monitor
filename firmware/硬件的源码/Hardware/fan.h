/**
 *  文件名  : fan.h
 *  功能    : 风扇PWM + 气泵GPIO 驱动头文件
 *  接口    : 风扇 PA8(TIM1_CH1 PWM)  气泵 PA9(GPIO 推挽输出)
 **/
#ifndef _FAN_H_
#define _FAN_H_

#include "stm32f10x.h"

/* 风扇控制宏 (0=关, 100=全速) */
#define FAN_ON      100
#define FAN_OFF     0

/* 气泵控制宏 (用于Aerator_Set参数) */
#define AERATOR_ON  1      /* HIGH → MOS导通 → 气泵开 */
#define AERATOR_OFF 0      /* LOW  → MOS截止 → 气泵关 */

/* 全局状态变量 */
extern uint8_t Fan_Status;        /* 风扇运行状态 (1=开 0=关) */
extern uint8_t Aerator_Status;    /* 气泵运行状态 (1=开 0=关) */

/* 函数声明 */
void Fan_Init(void);              /* 风扇PWM初始化 (TIM1_CH1) */
void Fan_Set(uint8_t status);     /* 风扇开关 (FAN_ON/FAN_OFF) */
void Fan_Adj(uint8_t value);      /* 风扇调速 (0~100%) */

void Aerator_Init(void);          /* 气泵GPIO初始化 (PA9) */
void Aerator_Set(uint8_t status); /* 气泵开关 (AERATOR_ON/OFF) */

#endif

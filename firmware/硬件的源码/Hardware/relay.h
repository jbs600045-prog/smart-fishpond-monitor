/**
 *  文件名  : relay.h
 *  功能    : 水泵继电器驱动头文件
 *  接口    : PA10 → 继电器 (低电平触发吸合)
 **/
#ifndef _RELAY_H_
#define _RELAY_H_

#include "stm32f10x.h"

/* 继电器控制宏 */
#define RELAY_PUMP_ON   1      /* 吸合 → 水泵通电运行 */
#define RELAY_PUMP_OFF  0      /* 断开 → 水泵断电停止 */

/* 全局状态变量 */
extern uint8_t Relay_Pump_Status;    /* 水泵继电器状态 (1=吸合 0=断开) */

/* 函数声明 */
void Relay_Init(void);              /* 继电器GPIO初始化 */
void Relay_Pump_Set(uint8_t status);/* 水泵开关控制 */

#endif

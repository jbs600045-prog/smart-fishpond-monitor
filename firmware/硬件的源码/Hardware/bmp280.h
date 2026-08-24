#ifndef _BMP280_H_
#define _BMP280_H_

#include "stm32f10x.h"

/* BMP280 气压传感器 — 软件I2C接口
 * SCL: PB1
 * SDA: PB0
 * I2C地址: 0x76 (SDO接GND)
 */

/* ---- 软件I2C引脚控制 ---- */
#define BMP280_SCL_H()  GPIO_SetBits(GPIOB, GPIO_Pin_1)
#define BMP280_SCL_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_1)
#define BMP280_SDA_H()  GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define BMP280_SDA_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define BMP280_SDA_IN() GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0)

/* ---- 函数声明 ---- */
void BMP280_Init(void);
uint8_t BMP280_Check(void);
float BMP280_Read_Temperature(void);  /* 返回温度 °C */
float BMP280_Read_Pressure(void);     /* 返回气压 kPa */

#endif
int32_t BMP280_ReadRawPress(void);

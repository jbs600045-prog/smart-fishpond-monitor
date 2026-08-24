#ifndef _DS18B20_H_
#define _DS18B20_H_

#include "stm32f10x.h"

/* DS18B20 接口: PA4 */
#define DS18B20_IO_IN()   {GPIOA->CRL&=0xFFF0FFFF;GPIOA->CRL|=8<<16;}
#define DS18B20_IO_OUT()  {GPIOA->CRL&=0xFFF0FFFF;GPIOA->CRL|=3<<16;}

#define DS18B20_DQ_OUT(x) GPIO_WriteBit(GPIOA, GPIO_Pin_4, (BitAction)x)
#define DS18B20_DQ_IN     GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4)

uint8_t DS18B20_Init(void);
float DS18B20_Read_Temp(void);
uint8_t DS18B20_Check(void);
void DS18B20_Rst(void);
uint8_t DS18B20_Read_Byte(void);
uint8_t DS18B20_Read_Bit(void);

#endif

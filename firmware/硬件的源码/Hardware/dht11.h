#ifndef DHT11_H
#define DHT11_H

#include "stm32f10x.h"


//PB8
#define DHT11_IO_IN()  {GPIOB->CRH&=0XFFFFFFF0;GPIOB->CRH|=8;}
#define DHT11_IO_OUT() {GPIOB->CRH&=0XFFFFFFF0;GPIOB->CRH|=3;}
//IO��������   
#define	DHT11_DQ_OUT(X)  GPIO_WriteBit(GPIOB, GPIO_Pin_8, X)
#define	DHT11_DQ_IN  GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8)

uint8_t DHT11_Init(void);//��ʼ��DHT11
uint8_t DHT11_Read_Data(uint8_t *temp,uint8_t *humi);//��ȡ����
uint8_t DHT11_Read_Byte(void);//��ȡһ���ֽ�
uint8_t DHT11_Read_Bit(void);//��ȡһλ
uint8_t DHT11_Check(void);//���DHT11
void DHT11_Rst(void);//��λDHT11   

#endif

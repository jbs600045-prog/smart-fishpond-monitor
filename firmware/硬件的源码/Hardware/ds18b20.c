/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	ds18b20.c
	*
	*	作者： 		金波胜
	*
	*	日期： 		2026-6-16
	
	 *  功能    : DS18B20 水温传感器驱动 (OneWire协议)
	 *  接口    : PA4 - DATA
	 *  说明    : 用于鱼塘/温室水温采集
	 ************************************************************
**/

#include "ds18b20.h"
#include "delay.h"

/* 复位DS18B20 */
void DS18B20_Rst(void)
{
    DS18B20_IO_OUT();
    DS18B20_DQ_OUT(0);          /* 拉低DQ */
    DelayUs(500);               /* 保持480us以上 */
    DS18B20_DQ_OUT(1);          /* 释放DQ */
    DelayUs(60);                /* 等待15~60us */
}

/* 检测DS18B20应答 */
uint8_t DS18B20_Check(void)
{
    uint8_t retry = 0;
    DS18B20_IO_IN();
    while(DS18B20_DQ_IN && retry < 240)     /* DS18B20拉低60~240us */
    {
        retry++;
        DelayUs(1);
    }
    if(retry >= 240) return 1;
    else retry = 0;
    while(!DS18B20_DQ_IN && retry < 240)    /* DS18B20释放总线 */
    {
        retry++;
        DelayUs(1);
    }
    if(retry >= 240) return 1;
    return 0;
}

/* 读取一个位 */
uint8_t DS18B20_Read_Bit(void)
{
    uint8_t data = 0;
    DS18B20_IO_OUT();
    DS18B20_DQ_OUT(0);
    DelayUs(2);                 /* 拉低2us */
    DS18B20_IO_IN();
    DelayUs(8);                 /* 延时读取 */
    if(DS18B20_DQ_IN) data = 1;
    DelayUs(50);                /* 等待时隙结束 */
    return data;
}

/* 读取一个字节 */
uint8_t DS18B20_Read_Byte(void)
{
    uint8_t i, dat = 0;
    for(i = 0; i < 8; i++)
    {
        dat >>= 1;
        if(DS18B20_Read_Bit()) dat |= 0x80;
    }
    return dat;
}

/* 写一个字节 */
static void DS18B20_Write_Byte(uint8_t dat)
{
    uint8_t i;
    DS18B20_IO_OUT();
    for(i = 0; i < 8; i++)
    {
        if(dat & 0x01)
        {
            DS18B20_DQ_OUT(0);
            DelayUs(2);
            DS18B20_DQ_OUT(1);
            DelayUs(60);
        }
        else
        {
            DS18B20_DQ_OUT(0);
            DelayUs(60);
            DS18B20_DQ_OUT(1);
            DelayUs(2);
        }
        dat >>= 1;
    }
}

/* 读取温度 (返回: °C) */
float DS18B20_Read_Temp(void)
{
    uint8_t TL, TH;
    int16_t temp_raw;

    DS18B20_Rst();
    if(DS18B20_Check()) return -99.0;

    DS18B20_Write_Byte(0xCC);   /* 跳过ROM */
    DS18B20_Write_Byte(0x44);   /* 启动温度转换 */

    DelayMs(200);               /* 10-bit分辨率转换最长187.5ms */

    DS18B20_Rst();
    if(DS18B20_Check()) return -99.0;

    DS18B20_Write_Byte(0xCC);   /* 跳过ROM */
    DS18B20_Write_Byte(0xBE);   /* 读暂存器 */

    TL = DS18B20_Read_Byte();
    TH = DS18B20_Read_Byte();

    temp_raw = (int16_t)((TH << 8) | TL);

    return (float)temp_raw / 16.0f;
}

/* 初始化DS18B20 (配置为10-bit分辨率, 0.25°C, 转换时间~187.5ms) */
uint8_t DS18B20_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    DS18B20_Rst();
    if(DS18B20_Check()) return 1;

    /* 配置分辨率: 10-bit (0x20 = R1=1, R0=0) */
    DS18B20_Write_Byte(0xCC);   /* 跳过ROM */
    DS18B20_Write_Byte(0x4E);   /* 写暂存器 */
    DS18B20_Write_Byte(0x00);   /* TH (不使用) */
    DS18B20_Write_Byte(0x00);   /* TL (不使用) */
    DS18B20_Write_Byte(0x3F);   /* 配置寄存器: 10-bit (R1=1, R0=0, 其余为1) */
    DelayMs(10);

    DS18B20_Rst();
    if(DS18B20_Check()) return 1;

    /* 复制暂存器到EEPROM, 掉电保持 */
    DS18B20_Write_Byte(0xCC);   /* 跳过ROM */
    DS18B20_Write_Byte(0x48);   /* 复制暂存器 */
    DelayMs(50);                /* EEPROM写入需要时间 */

    return 0;
}

/**
 ************************************************************
 *  文件名  : ph_sensor.c
 *  作者    : 金波胜
 *  日期    : 2026-6-16
 *  接口    : PA1 - ADC1_CH1 (共用ADC模组)
 *  说明    : pH传感器驱动 (模拟量采集, ADC1_CH1)
 *            硬件板子 P0→10K→PA1→10K→GND, 分压比1/2需补偿
 *            标定: 清水(约pH7)分压前电压≈1.28V, 斜率≈0.06V/pH
 ************************************************************
**/

#include "ph_sensor.h"
#include "adc_common.h"

void PH_Init(void)
{
    /* ADC由ADC_Common_Init统一初始化, 此处无需操作 */
}

float PH_Read(void)
{
    uint16_t adc_val;
    float vol, ph;

    adc_val = ADC_Read_CH1();  /* PA1 → pH传感器 */
    vol = (float)adc_val * 3.3f / 4095.0f;
    ph = 7.0f + (vol - 0.64f) / 0.06f;
    if(ph < 0.0f) ph = 0.0f;
    if(ph > 14.0f) ph = 14.0f;
    return ph;
}

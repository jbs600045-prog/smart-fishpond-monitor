/**
 ************************************************************
 *  文件名  : mq2.c
 *  作者    : 金波胜
 *  日期    : 2026-6-13
 *  说明    : MQ2空气质量传感器驱动 (ADC1_CH0)
 *  接口    : PA0 - ADC1_CH0 (共用ADC模组)
 *  说明    : 使用ADC_Common模块读取, 转换为电压值
 ************************************************************
**/

#include "mq2.h"
#include "adc_common.h"
#include "usart.h"

void Mq2_Init(void)
{
    /* ADC由ADC_Common_Init统一初始化, 此处无需操作 */
}

/**
 * adcValueToVoltage: MQ2 ADC采样 → 烟雾浓度 (0~10)
 * 说明: MQ2模块输出 0~3.3V, 经线性映射到 0~10 量程
 *       实际使用时需根据MQ2传感器标定曲线校准
 */
float adcValueToVoltage(void)
{
    uint16_t adc_value;
    float smoke_level;

    adc_value = ADC_Read_CH0();  /* PA0 → MQ2 */

    /* ADC值 → 电压 → 烟雾浓度(0~10) */
    smoke_level = (float)adc_value * 10.0f / 4095.0f;

    if(smoke_level < 0.0f)  smoke_level = 0.0f;
    if(smoke_level > 10.0f) smoke_level = 10.0f;

    return smoke_level;
}

float estimateSmokeLevel(float voltage)
{
    /* 保留旧接口兼容: 电压0~3.3V → 0~10 */
    return voltage / 3.3f * 10.0f;
}

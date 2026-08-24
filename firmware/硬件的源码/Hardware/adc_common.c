/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	adc_common.c
	*
	*	作者： 		金波胜
	* 	功能：    	ADC1双通道共用模块
	*	日期： 		2026-6-10
					PA0(ADC1_CH0) = MQ2空气质量
 *           		PA1(ADC1_CH1) = pH传感器

	*************************************************************
	************************************************************
	************************************************************
 ************************************************************
 *  文件名  : adc_common.c
 * 
 *  接口    : PA0(ADC1_CH0) = MQ2空气质量
 *           PA1(ADC1_CH1) = pH传感器
 *  说明    : 两个传感器分时复用ADC1, 切换通道后读取
 ************************************************************
**/

#include "adc_common.h"

static uint8_t adc_initialized = 0;

void ADC_Common_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    if(adc_initialized) return;
    adc_initialized = 1;

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  /* ADC时钟12MHz */

    /* PA0 + PA1 模拟输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ADC1配置: 独立模式, 单次转换(软件触发切换通道) */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 使能ADC1并校准 */
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

/**
 * ADC_Read_CH0: 读取PA0 (MQ2空气质量)
 * 返回: 12位ADC原始值 (0~4095)
 */
uint16_t ADC_Read_CH0(void)
{
    /* 切换到CH0 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);

    /* 启动转换并等待完成 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

    return ADC_GetConversionValue(ADC1);
}

/**
 * ADC_Read_CH1: 读取PA1 (pH传感器)
 * 返回: 12位ADC原始值 (0~4095)
 * 说明: 多次采样取平均, 滤除噪声(pH探头内阻高易受干扰)
 */
uint16_t ADC_Read_CH1(void)
{
    uint32_t sum = 0;
    uint8_t i;

    /* 切换到CH1, 最大采样时间 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

    /* 连续采样4次取平均, 每次采样间延时1ms让引脚电压稳定 */
    for(i = 0; i < 4; i++)
    {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        sum += ADC_GetConversionValue(ADC1);
        DelayXms(1);
    }

    return (uint16_t)(sum / 4);
}

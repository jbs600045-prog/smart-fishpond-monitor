/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	key.c
	*
	*	作者： 		金波胜
	*
	*	日期： 		2026-6-16
	*
	*************************************************************
	************************************************************
	************************************************************
**/
//单片机头文件
#include "stm32f10x.h"

//硬件驱动
#include "key.h"
#include "delay.h"
#include "beep.h"
#include "fan.h"


/*
************************************************************
*	函数名称：	Key_Init
*
*	函数功能：	按键初始化
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
************************************************************
*/
void Key_Init(void)
{

	GPIO_InitTypeDef gpio_initstruct;
	EXTI_InitTypeDef exti_initstruct;
	NVIC_InitTypeDef nvic_initstruct;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//打开GPIOB的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IPU;				//设置为输出
	gpio_initstruct.GPIO_Pin = GPIO_Pin_12;						//将初始化的Pin脚
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;				//可承载的最大频率
	GPIO_Init(GPIOB, &gpio_initstruct);							//初始化GPIOB
	
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
	
	exti_initstruct.EXTI_Line = EXTI_Line12;
	exti_initstruct.EXTI_Mode = EXTI_Mode_Interrupt;
	exti_initstruct.EXTI_Trigger = EXTI_Trigger_Falling;
	exti_initstruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&exti_initstruct);
	
	nvic_initstruct.NVIC_IRQChannel = EXTI15_10_IRQn;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 2;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 2;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_initstruct);
	
}



void EXTI15_10_IRQHandler(void)
{
	DelayXms(10);
	if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12)==0)
	{
		if(Fan_Status==1) 
		{	
			Fan_Set(FAN_OFF);
		}
		else 
		{
			Fan_Set(FAN_ON);
		}
		
		Beep_Set(BEEP_OFF);
				

	}
	EXTI_ClearITPendingBit(EXTI_Line12);
}


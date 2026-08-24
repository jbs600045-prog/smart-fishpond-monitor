/**
 *  文件名  : fan_pwm.c
 *  作者    : 金波胜
 *  日期    : 2026-6-16
 *  功能    : TIM1 PWM底层配置 (风扇控制器)
 *
 *  PA8 → TIM1_CH1 → 风扇 PWM
 *  PWM频率 = 72MHz / (72预分频 × 1000周期) = 1kHz
 *  占空比 = CCR1 / 999(ARR) × 100%
 **/
#include "fan_pwm.h"

/* 函数前置声明 */
void RCC_Configuration(void);
void GPIO_Configuration(void);
void TIM1_Configuration(void);

/*================================================================
 *  RCC_Configuration — 使能TIM1和GPIOA时钟
 *================================================================*/
void RCC_Configuration(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);	//TIM1
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//GPIOA
}

/*================================================================
 *  GPIO_Configuration — PA8配置为TIM1_CH1复用推挽输出
 *================================================================*/
void GPIO_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* PA8 → TIM1_CH1 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;      //复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*================================================================
 *  TIM1_Configuration — TIM1时基 + CH1 PWM输出配置
 *
 *  时基:  预分频=72-1, 自动重载=1000-1 → 1kHz PWM频率
 *  CH1:   PWM模式1, 高电平有效, CCR初始=0(0%占空比)
 *
 *  TIM1是高级定时器, 需要额外使能MOE主输出
 *================================================================*/
void TIM1_Configuration(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    /*---------- 时基配置 ----------*/
    TIM_TimeBaseStructure.TIM_Period        = 1000 - 1;     /* ARR=999, 计数0~999 */
    TIM_TimeBaseStructure.TIM_Prescaler     = 72 - 1;       /* PSC=71, 72MHz÷72=1MHz */
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /*---------- CH1 PWM输出配置 (风扇) ----------*/
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM1, ENABLE);

    /*---------- 启动TIM1 ----------*/
    TIM_Cmd(TIM1, ENABLE);              /* 计数器使能 */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);   /* MOE主输出使能 (TIM1/8必须) */
}

/**
 *  文件名  : relay.c
 *  作者    : 金波胜
 *  日期    : 2026-6-16
 *  功能    : 水泵继电器控制 (PA10)
 *
 *  硬件接口: PA10 → 继电器模块 (低电平触发)
 *            PA10 = LOW  → 光耦导通 → 继电器吸合 → 水泵通电
 *            PA10 = HIGH → 光耦截止 → 继电器断开 → 水泵断电
 *
 *  
 **/

#include "relay.h"

uint8_t Relay_Pump_Status = 0;    /* 水泵继电器状态: 1=吸合(开) 0=断开(关) */

/*================================================================
 *  Relay_Init — 继电器初始化
 *  配置 PA10 为推挽输出, 初始输出 HIGH (断开)
 *================================================================*/
void Relay_Init(void)
{
    GPIO_InitTypeDef gpio_initstruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);  /* 使能GPIOA时钟 */

    /* PA10 = 水泵继电器控制引脚 */
    gpio_initstruct.GPIO_Mode  = GPIO_Mode_Out_PP;        /* 推挽输出 */
    gpio_initstruct.GPIO_Pin   = GPIO_Pin_10;
    gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_initstruct);

    /* 初始关闭: HIGH → 继电器断开 */
    GPIO_SetBits(GPIOA, GPIO_Pin_10);
    Relay_Pump_Status = RELAY_PUMP_OFF;
}

/*================================================================
 *  Relay_Pump_Set — 水泵开关
 *  status: RELAY_PUMP_ON → PA10=LOW(吸合), RELAY_PUMP_OFF → PA10=HIGH(断开)
 *================================================================*/
void Relay_Pump_Set(uint8_t status)
{
    if(status == RELAY_PUMP_ON)
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_10);   /* PA10=LOW → 吸合 → 水泵开 */
        Relay_Pump_Status = 1;
    }
    else
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_10);     /* PA10=HIGH → 断开 → 水泵关 */
        Relay_Pump_Status = 0;
    }
}

/**
 *  文件名  : fan.c
 *  作者    : 金波胜
 *  日期    : 2026-6-16
 *  功能    : 风扇PWM调速 + 气泵GPIO开关
 *
 *  ========== 硬件接口 ==========
 *  风扇: PA8  → TIM1_CH1 (PWM, 0~100% 占空比)
 *         PWM频率 = 72MHz / 72 / 1000 = 1kHz
 *         占空比 = CCR1 / 999 (ARR) × 100%
 *         起动: 先给100%占空比20ms, 再调到目标值 (防止电机堵转)
 *
 *  气泵: PA9  → GPIO 推挽输出
 *         PA9 = HIGH → 外接N-MOS栅极高电平 → MOS导通 → 气泵通电运行
 *         PA9 = LOW  → MOS截止 → 气泵停转
 *
 *  ========== 函数说明 ==========
 *  Fan_Init()     : 初始化TIM1时钟+GPIO+CH1 PWM (由fan_pwm.c子模块实现)
 *  Fan_Set(val)   : 风扇开关, val=100→全速, 0→关闭
 *  Fan_Adj(val)   : 风扇调速, val范围 0~100%
 *  Aerator_Init() : 气泵GPIO初始化, 初始关闭
 *  Aerator_Set(v) : 气泵开关, AERATOR_ON→开(HIGH), AERATOR_OFF→关(LOW)
 *
 *  ========== 全局变量 ==========
 *  Fan_Status      : 风扇运行状态 (1=开, 0=关)
 *  Aerator_Status  : 气泵运行状态 (1=开, 0=关)
 *  fan_adj         : 风扇当前调速值 (main.c全局, 用于OLED显示和OneNET上传)
 **/

/* 硬件驱动头文件 */
#include "fan.h"          /* 本模块头文件 (宏定义+函数声明) */
#include "delay.h"        /* DelayXms() 毫秒延时 */
#include "fan_pwm.h"      /* TIM1 PWM 底层初始化 (RCC+GPIO+TIM1配置) */

/*---- 全局状态变量 ----*/
uint8_t Fan_Status = 0;       /* 风扇开关状态: 1=运行, 0=停止 */

/*---- 引用main.c全局变量 ----*/
extern u8 fan_adj;            /* 风扇调速值 (main.c定义, OLED/OneNET共用) */

/*================================================================
 *  Fan_Init — 风扇PWM初始化
 *  调用fan_pwm.c的三个子函数完成TIM1配置
 *================================================================*/
void Fan_Init(void)
{
    RCC_Configuration();       /* 使能TIM1 + GPIOA时钟 */
    GPIO_Configuration();      /* PA8配置为AF_PP (TIM1_CH1) */
    TIM1_Configuration();      /* TIM1时基+PWM模式+CH1输出使能 */
}

/*================================================================
 *  Fan_Adj — 风扇PWM调速控制
 *  value: 0~100, 对应占空比 0%~100%
 *  起动策略: 先给100%占空比起动20ms, 再调到目标值
 *            (风扇电机需要较大起动力矩克服静摩擦)
 *================================================================*/
void Fan_Adj(uint8_t value)
{
    uint16_t ccr;             /* CCR1比较值 = value × 10 */

    if(value > 99) value = 99; /* 限制最大99%, 避免CCR=1000 > ARR=999 */

    TIM_SetCompare1(TIM1, 999); /* 第一步: 100%占空比起动 */
    DelayXms(20);               /* 等待20ms让风扇转起来 */

    ccr = (uint16_t)value * 10; /* 第二步: 计算目标CCR (value% × 999/100 ≈ value×10) */
    TIM_SetCompare1(TIM1, ccr); /* 写入CCR1 → 更新占空比 */

    /* PWM切换后加延时, 等噪声衰减后再允许ADC采样(避免干扰pH读数) */
    if(value > 0)
        DelayXms(5);

    if(value > 0)
    {
        Fan_Status = 1;        /* 标记风扇运行 */
        fan_adj = value;       /* 同步到全局变量 (供OLED显示+OneNET上传) */
    }
    else
    {
        Fan_Status = 0;        /* 标记风扇停止 */
        fan_adj = 0;
    }
}

/*================================================================
 *  Fan_Set — 风扇开关 (FAN_ON=100, FAN_OFF=0)
 *================================================================*/
void Fan_Set(uint8_t value)
{
    Fan_Adj(value);            /* 直接调用调速函数 */
}


/*################################################################
 *     气泵 PWM控制 TIM1_CH2 PA9 (开漏输出)
 *     CCR2=999 → 100%占空比 → 气泵开

*################################################################
 *     气泵 GPIO PA15
 *################################################################*/

uint8_t Aerator_Status = 0;

void Aerator_Init(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
	
    GPIO_ResetBits(GPIOA, GPIO_Pin_9);
    Aerator_Status = AERATOR_OFF;
}

void Aerator_Set(uint8_t status)
{
    if(status == AERATOR_ON)
    {
       
		GPIO_SetBits(GPIOA, GPIO_Pin_9);
        Aerator_Status = 1;
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_9);
        Aerator_Status = 0;
    }
}

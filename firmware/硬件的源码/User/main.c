/**
 ****************************************************************************
 *  项目名称 : 智慧农业——远程鱼塘/温室环境控制系统
 *  主控芯片 : STM32F103C8T6 
 *  开发环境 : Keil MDK V5.06 + 标准外设库 V3.5.0
 *	指导老师 ：苏琦
 *  作者	 : 金波胜  			   
 *  日期     : 2026-06
 *
 *  ========== 硬件引脚分配 ==========
 *  传感器:
 *    PA4      - DS18B20 水温传感器 (单总线)
 *    PA0(ADC) - MQ2 烟雾/空气质量传感器 (0~3.3V → 0~10量程)
 *    PA1(ADC) - pH 传感器 (0~3.3V → 0~14pH)
 *    PB8      - DHT11 空气温湿度传感器 (单总线)
 *    PB0/PB1  - BMP280 气压传感器 I2C
 *
 *  执行器:
 *    PA9(GPIO)- 气泵/增氧机 (外接N-MOS电子开关模块, HIGH=开/LOW=关)
 *    PA10     - 水泵继电器 (低电平触发吸合)
 *    PA8(TIM1)- 风扇 PWM 调速 (0~100%)
 *    PB9      - 蜂鸣器 (报警输出)
 *
 *  通信:
 *    PA2      - USART2_TX → ESP8266 RX (WiFi模块)
 *    PA3      - USART2_RX → ESP8266 TX
 *    PB6/PB7  - USART1 调试串口 (重映射, 仅开发调试用)
 *
 *  显示:
 *    PB10     - OLED I2C SDA 
 *    PB11     - OLED I2C SCL
 *
 *  ========== 功能概述 ==========
 *  1. 每秒采集5路传感器数据 (水温/气压/pH/温湿度/烟雾)
 *  2. 亨利定律公式计算溶解氧 (水温+气压→饱和DO)
 *  3. 自动控制模式: 根据阈值自动开关增氧机/水泵/风扇
 *  4. 手动控制模式: 接收App指令远程控制所有设备
 *  5. 4项阈值报警: 溶解氧过低/pH异常/水温异常/烟雾超标 → 蜂鸣器
 *  6. 每3秒MQTT上传19项数据到OneNET云平台
 *  7. OLED 3页轮播: 水质参数/环境参数/设备状态
 *  8. App远程控制 + 阈值调节 + 报警推送
 ****************************************************************************
**/
/******************************************************************************
 *                                                                            
 *        	     /\     佛祖保佑           |\_/|                                  
 *              /  \    代码无BUG          |^_^|                                  
 *       	   	 /|||\    阿弥陀佛          /     \                                 
 *      	    _|||||_   法力无边        _/       \_                               
 *                                                                            
 *                       _oo0oo_                                             
 *                      o8888888o                                            
 *                      88" . "88                                            
 *                      (| -_- |)                                            
 *                      0\  =  /0                                            
 *                    ___/`---'\___                                          
 *                  .' \\|     |// '.                                        
 *                 / \\|||  :  |||// \                                       
 *                / _||||| -:- |||||- \                                      
 *               |   | \\\  -  /// |   |                                     
 *               | \_|  ''\---/''  |_/ |                                     
 *               \  .-\__  '-'  ___/-. /                                     
 *             ___'. .'  /--.--\  `. .'___                                   
 *          ."" '<  `.___\_<|>_/___.' >' "".                                 
 *         | | :  `- \`.;`\ _ /`;.`/ - ` : | |                               
 *         \  \ `_.   \_ __\ /__ _/   .-` /  /                               
 *     =====`-.____`.___ \_____/___.-`___.-'=====                            
 *                       `=---='                                             
 *                                                                            
 *     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                           
 *         如来保佑     永无BUG        法力无边                           
 *                                                                            
 *****************************************************************************/

/*========== 单片机标准头文件 ==========*/
#include "stm32f10x.h"

/*========== 网络通信层 ==========*/
#include "onenet.h"        /* OneNET MQTT 协议封装 */
#include "esp8266.h"       /* ESP8266 WiFi模块 AT指令驱动 */

/*========== 硬件驱动层 ==========*/
#include "Delay.h"         /* SysTick 微秒/毫秒延时 */
#include "usart.h"         /* 串口初始化与格式化输出 */
#include "beep.h"          /* 蜂鸣器驱动 PB9 */
#include "fan.h"           /* 风扇PWM + 气泵GPIO驱动 */
#include "key.h"           /* 按键驱动 (预留) */
#include "dht11.h"         /* DHT11 温湿度传感器 PB8 */
#include "oled.h"          /* 0.96寸 OLED 显示屏 I2C */
#include "mq2.h"           /* MQ2 烟雾传感器 ADC初始化 */
#include "ds18b20.h"       /* DS18B20 水温传感器 PA4 */
#include "bmp280.h"        /* BMP280 气压传感器 */
#include "do_estimate.h"   /* 亨利定律溶解氧估算算法 */
#include "ph_sensor.h"     /* pH传感器 PA1 ADC */
#include "relay.h"         /* 继电器驱动 (水泵 PA10) */
#include "adc_common.h"    /* ADC共用模块 (PA0/PA1) */

/*========== C标准库 ==========*/
#include <string.h>
#include <stdio.h>

/*========== OneNET MQTT 服务器连接命令 ==========*/
#define ESP8266_ONENET_INFO  "AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"

/*========== 函数声明 ==========*/
void Hardware_Init(void);          /* 硬件初始化 */
void Display_Init(void);           /* OLED显示初始化 */
void Refresh_Data(void);           /* OLED数据刷新 */
void AutoControl_Check(void);      /* 自动控制逻辑 (auto_mode=1时调用) */
void Alarm_Check(void);            /* 阈值报警检测 */
void OLED_ShowPage(u8 page);       /* OLED页面切换 */

/*#############################################################################
 *                           全 局 变 量 定 义
 *#############################################################################*/

/*---- 传感器原始数据 ----*/
float water_temp;               /* 水温 (°C) — DS18B20 测量值 */
float bmp_pressure;             /* 大气压 (kPa) — BMP280 或固定101.3 */
float bmp_temp;                 /* BMP280芯片温度 (°C) — 仅调试参考 */
float do_value;                 /* 溶解氧 (mg/L) — 亨利定律估算值 */
float ph_value;                 /* pH值 — 0~14 */
u8 air_temp, air_humi;       /* 空气温度(°C) / 湿度(%) — DHT11 */
float mq2_vol;                 /* 烟雾浓度 (0~10量程) — MQ2 ADC转换 */

/*---- 传感器在线状态 (1=正常, 0=离线) ----*/
u8 sensor_ok_water_temp = 0;   /* DS18B20 在线标志 */
u8 sensor_ok_bmp280     = 0;   /* BMP280 在线标志 (暂时强制为1) */
u8 sensor_ok_dht11      = 0;   /* DHT11 在线标志 */

/*---- 执行器状态 ----*/
u8 aerator_status;              /* 气泵状态: 1=开(HIGH) 0=关(LOW) */
u8 pump_status;                 /* 水泵状态: 1=开(继电器吸合) 0=关 */
u8 fan_adj;                     /* 风扇调速: 0=关, 1~100=不同转速 */
u8 auto_mode;                   /* 控制模式: 1=自动控制 0=手动(App控制) */

/*---- 系统运行变量 ----*/
u8 disp_page;                   /* OLED 当前显示页面: 0=水质 1=环境 2=设备 */
u8 alarm_flag;                  /* 报警标志: 1=蜂鸣器响 0=正常 */

/*#############################################################################
 *                      阈 值 定 义 (可被App远程修改来看效果)
 *#############################################################################*/

/* 注意: 溶解氧估算值为理论饱和DO, 需根据实际鱼塘情况校准阈值 */
#define DO_LOW_TH_DEFAULT           4.0f   /* 溶解氧过低 → 自动开增氧机 + 报警 */
#define DO_HIGH_TH_DEFAULT          7.0f   /* 溶解氧达标 → 关增氧机  */
#define PH_LOW_TH_DEFAULT           6.5f   /* pH过低报警阈值 */
#define PH_HIGH_TH_DEFAULT          8.5f   /* pH过高报警阈值 (此阈值App不可调) */
#define WATER_TEMP_HIGH_TH_DEFAULT  30.0f  /* 水温过高 → 开水泵循环降温 */
#define WATER_TEMP_LOW_TH_DEFAULT   2.0f   /* 水温过低报警 (此阈值App不可调) */
#define AIR_TEMP_HIGH_TH_DEFAULT    38.0f  /* 气温过高 → 开风扇通风 */
#define AIR_TEMP_VERY_HIGH          42.0f  /* 极端高温 → 风扇全速100% */
#define AIR_TEMP_LOW_TH_DEFAULT     10.0f  /* 气温过低报警 */
#define SMOG_TH_DEFAULT             0.4f   /* 烟雾浓度阈值 (0~10量程) */

/*---- 可被App远程修改的阈值变量 ----*/
float do_low_th           = DO_LOW_TH_DEFAULT;			//溶解氧低阀值
float do_high_th          = DO_HIGH_TH_DEFAULT;			/* 溶解氧高阈值 */
float ph_low_th           = PH_LOW_TH_DEFAULT;			//PH低阀值
float ph_high_th          = PH_HIGH_TH_DEFAULT;			/* pH高阈值 */
float water_temp_high_th  = WATER_TEMP_HIGH_TH_DEFAULT;		//水温高阀值
float water_temp_low_th   = WATER_TEMP_LOW_TH_DEFAULT;		/* 水温低阈值 */
float air_temp_high_th    = AIR_TEMP_HIGH_TH_DEFAULT;		//气温高阀值
float air_temp_low_th     = AIR_TEMP_LOW_TH_DEFAULT;		//气温低阀值
float smog_th             = SMOG_TH_DEFAULT;				//烟雾（空气质量）阀值

/*#############################################################################
 *                    主 函 数 — 系统入口
 *#############################################################################*/
int main(void)
{
    /*---- 循环计数器 ----*/
    unsigned short timeCount = 0;      /* 数据上传间隔计数 (每150次≈3秒) */
    unsigned short sensorCount = 0;    /* 传感器读取间隔计数 (每100次≈1秒) */
    unsigned char *dataPtr = NULL;     /* 接收ESP8266 MQTT消息的指针 */

    /*========== 第1步: 硬件初始化 ==========*/
    Hardware_Init();

    /*========== 第2步: ESP8266连接WiFi ==========*/
    ESP8266_Init();

    /*========== 第3步: 连接OneNET MQTT服务器 ==========*/
    OLED_Clear();
    OLED_ShowString(0, 0, "Smart Fish Pond", 16);    /* 项目名 */
    OLED_ShowString(0, 2, "Connect Cloud...", 16);   /* 正在连接云平台 */
    DelayXms(500);

    /* 建立TCP连接到OneNET MQTT服务器 (mqtts.heclouds.com:1883) */
    while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))
        DelayXms(500);
    OLED_ShowString(0, 4, "Cloud Connected!", 16);    /* 连接成功 */
    DelayXms(500);

    /*========== 第4步: MQTT设备登录 + 订阅控制主题 ==========*/
    OLED_Clear();
    OLED_ShowString(0, 0, "Device login ...", 16);    /* 正在登录 */
    while(OneNet_DevLink())          /* MQTT CONNECT + 鉴权, 失败自动重连 */
    {
        ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT");  /* 重连TCP */
        DelayXms(500);
    }

    OneNET_Subscribe();              /* 订阅 $sys/.../thing/property/set */
                                     /* 用于接收App下发的控制指令 */

    Display_Init();                  /* OLED初始显示 */
    /* WiFi已连接, 安全初始化大功率设备 */
    Fan_Init();
    Aerator_Init();
    Fan_Set(FAN_OFF);

  

    /*#########################################################################
     *                    主 循 环 (超级循环)
     *  每个周期 ≈ 10ms (DelayMs(10))
     *  - 传感器每秒采集一次 (sensorCount >= 100)
     *  - 数据每3秒上传一次 (timeCount >= 150)
     *  - OLED每秒刷新 + 翻页
     *  - 每周期检查MQTT消息
     *#########################################################################*/
    while(1)
    {
        /*========== [1] 传感器采集 (约1秒/次) ==========*/
        if(++sensorCount >= 100)     /* 100 × 10ms = 1秒 */
        {
            /*--- 水温 (DS18B20 PA4) ---*/
            water_temp = DS18B20_Read_Temp();
            if(water_temp > -50.0f && water_temp < 125.0f)
                sensor_ok_water_temp = 1;   /* 有效读数 */
            else
                sensor_ok_water_temp = 0;   /* 传感器离线/故障 */

            /*--- 大气压 (BMP280 PB0/PB1 I2C) ---*/
            bmp_pressure = (float)BMP280_ReadRawPress() * 0.0003017f;  /* 线性近似 */
            if(bmp_pressure < 50.0f) { DelayMs(50); bmp_pressure = (float)BMP280_ReadRawPress() * 0.0003017f; }
            bmp_temp     = 25.0f;
            if(bmp_pressure > 50.0f && bmp_pressure < 120.0f)
                sensor_ok_bmp280 = 1;
            else
                sensor_ok_bmp280 = 0;
            if(sensor_ok_water_temp && sensor_ok_bmp280)
                do_value = DO_Estimate_Read(water_temp, bmp_pressure);

            /*--- pH值 (PA1 ADC) ---*/
            ph_value = PH_Read();

            /*--- 空气温湿度 (DHT11 PB8) ---*/
            if(DHT11_Read_Data(&air_temp, &air_humi) == 0)
                sensor_ok_dht11 = 1;        /* 读取成功 */
            else
                sensor_ok_dht11 = 0;        /* 读取失败(校验错误) */

            /*--- 烟雾浓度 (MQ2 PA0 ADC → 0~10量程) ---*/
            mq2_vol = adcValueToVoltage();

            /*--- 自动控制 (仅在传感器正常 且 自动模式下执行) ---*/
            if(auto_mode && sensor_ok_water_temp && sensor_ok_bmp280)
                AutoControl_Check();

            /*--- 阈值报警检测 ---*/
            if(sensor_ok_water_temp && sensor_ok_bmp280)
                Alarm_Check();

            sensorCount = 0;           /* 重置传感器计数 */
        }

        /*========== [2] 处理App下发指令 (每周期检查) ==========*/
        dataPtr = ESP8266_GetIPD(3);   /* 检查ESP8266是否收到MQTT消息 */
        if(dataPtr != NULL)
            OneNet_RevPro(dataPtr);    /* 解析JSON并执行指令 */

        /*========== [3] OLED显示刷新 + 3页轮播 (传感器采集后立即刷新) ==========*/
        if(sensorCount == 0)           /* 每次传感器采集后的第一个周期 */
        {
            if(++disp_page >= 3) disp_page = 0;  /* 循环翻页: 0→1→2→0 */
            OLED_ShowPage(disp_page);            /* 绘制页面框架 */
            Refresh_Data();                      /* 填充实时数据 */
        }

        /*========== [4] 数据上传OneNET (约3秒/次) ==========*/
        if(++timeCount >= 150)         /* 150 × 10ms = 1.5秒 (考虑GetIPD延迟≈3秒) */
        {
            OneNet_SendData();         /* MQTT PUBLISH 上传19项数据 */
            timeCount = 0;             /* 重置上传计数 */
            ESP8266_Clear();           /* 清空ESP8266接收缓冲区 */

            /* 调试输出 (通过USART1 PB6/PB7, 波特率115200) */
            UsartPrintf(USART_DEBUG,
                "WT:%.1f P:%.1fkPa DO:%.1f pH:%.1f AT:%d AH:%d MQ:%.2f "
                "AER:%d PUMP:%d FAN:%d MODE:%d\r\n",
                water_temp, bmp_pressure, do_value, ph_value,
                air_temp, air_humi, mq2_vol,
                aerator_status, pump_status, fan_adj,
                auto_mode);
        }

        DelayMs(10);                   /* 主循环节拍 10ms */
    }
}

/*#############################################################################
 *                  Hardware_Init — 硬件初始化
 *  按顺序初始化所有外设, 任一传感器失败会循环重试
 *#############################################################################*/
void Hardware_Init(void)
{
    /*---- 中断优先级分组: 2位抢占 + 2位子优先级 ----*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    Delay_Init();                    /* SysTick 系统定时器 (延时用) */

    Usart1_Init(115200);             /* 调试串口1 PB6/PB7 (重映射) */
    Usart2_Init(115200);             /* ESP8266 通信串口2 PA2/PA3 */

    OLED_Init();                     /* 0.96寸 OLED I2C PB10/PB11 */

    Key_Init();                      /* 按键初始化 (预留) */

    Beep_Init();                     /* 蜂鸣器 PB9 */
    Beep_Set(BEEP_OFF);              /* 初始关闭 */

    /*---- DHT11 温湿度传感器 ----*/
    OLED_ShowString(0, 0, "Init DHT11...", 16);
    while(DHT11_Init())              /* 失败重试 */
    {
        OLED_ShowString(0, 2, "DHT11 Error", 16);
        DelayMs(1000);
    }

    /*---- DS18B20 水温传感器 ----*/
    OLED_ShowString(0, 2, "Init DS18B20...", 16);
    while(DS18B20_Init())            /* 失败重试 */
    {
        OLED_ShowString(0, 4, "DS18B20 Error", 16);
        DelayMs(1000);
    }

  
    OLED_ShowString(0, 2, "Init BMP280...", 16);
    BMP280_Init();
    while(BMP280_Check()) { OLED_ShowString(0, 4, "BMP280 Error", 16); DelayMs(1000); }

    /*---- 溶解氧估算模块 (纯软件计算, 无硬件) ----*/
    DO_Estimate_Init();

    /*---- ADC共用模块 (PA0=MQ2, PA1=pH) ----*/
    ADC_Common_Init();

    /*---- MQ2 烟雾传感器 ----*/
    Mq2_Init();

    /*---- pH 传感器 ----*/
    PH_Init();

    /*---- 水泵继电器 PA10 (低电平吸合, 初始断开) ----*/
    Relay_Init();

    /*---- 默认使用自动控制模式 ----*/
    auto_mode = 1;

    UsartPrintf(USART_DEBUG, " Hardware init OK\r\n");
    OLED_Clear();
    OLED_ShowString(0, 0, "Hardware init OK", 16);
    DelayMs(500);
}

/*#############################################################################
 *                  Display_Init — OLED初始显示
 *#############################################################################*/
void Display_Init(void)
{
    disp_page = 0;          /* 从第1页开始 */
    OLED_Clear();           /* 清屏 */
}

/*#############################################################################
 *         OLED_ShowPage — OLED页面框架 (3页轮播)
 *  page=0: 水温 + 溶解氧 + pH
 *  page=1: 气温 + 湿度 + 气压 + 空气质量
 *  page=2: 气泵 + 水泵 + 风扇 + 控制模式
 *#############################################################################*/
void OLED_ShowPage(u8 page)
{
    OLED_Clear();     /* 每次切换先清屏 */

    switch(page)
    {
        case 0:   /*===== 第1页: 水质参数 =====*/
            OLED_ShowCHinese(0, 0, 48);    /* 水 (字库索引48, 16×16) */
            OLED_ShowCHinese(16, 0, 1);    /* 温 (字库索引1) */
            OLED_ShowChar(32, 0, ':', 16); /* 冒号 (8×16 ASCII) */

            OLED_ShowCHinese(0, 2, 50);    /* 溶 */
            OLED_ShowCHinese(16, 2, 51);   /* 解 */
            OLED_ShowCHinese(32, 2, 52);   /* 氧 */
            OLED_ShowChar(48, 2, ':', 16);

            OLED_ShowString(0, 4, (u8*)"PH:", 16);  /* pH值 (ASCII标签) */
            break;

        case 1:   /*===== 第2页: 环境参数 =====*/
            OLED_ShowCHinese(0, 0, 49);    /* 气 */
            OLED_ShowCHinese(16, 0, 1);    /* 温 */
            OLED_ShowChar(32, 0, ':', 16);

            OLED_ShowCHinese(0, 2, 4);     /* 湿 (字库索引4) */
            OLED_ShowCHinese(16, 2, 2);    /* 度 (字库索引2) */
            OLED_ShowChar(32, 2, ':', 16);

            OLED_ShowCHinese(0, 4, 49);    /* 气 */
            OLED_ShowCHinese(16, 4, 54);   /* 压 */
            OLED_ShowChar(32, 4, ':', 16);

            OLED_ShowCHinese(0, 6, 53);    /* 空 */
            OLED_ShowCHinese(16, 6, 49);   /* 气 */
            OLED_ShowCHinese(32, 6, 55);   /* 质 */
            OLED_ShowCHinese(48, 6, 56);   /* 量 */
            OLED_ShowChar(64, 6, ':', 16);
            break;

        case 2:   /*===== 第3页: 设备状态 =====*/
            OLED_ShowCHinese(0, 0, 57);    /* 增 */
            OLED_ShowCHinese(16, 0, 52);   /* 氧 */
            OLED_ShowCHinese(32, 0, 58);   /* 机 */
            OLED_ShowChar(48, 0, ':', 16);

            OLED_ShowCHinese(0, 2, 48);    /* 水 */
            OLED_ShowCHinese(16, 2, 59);   /* 泵 */
            OLED_ShowChar(32, 2, ':', 16);

            OLED_ShowCHinese(0, 4, 60);    /* 风 */
            OLED_ShowCHinese(16, 4, 61);   /* 扇 */
            OLED_ShowChar(32, 4, ':', 16);

            OLED_ShowCHinese(0, 6, 62);    /* 模 */
            OLED_ShowCHinese(16, 6, 63);   /* 式 */
            OLED_ShowChar(32, 6, ':', 16);
            break;

        default: break;
    }
}

/*#############################################################################
 *  Refresh_Data — 刷新OLED数据值 (在OLED_ShowPage之后调用)
 *  将全局变量转换为字符串并显示在对应位置
 *  使用8×16 ASCII字体, 单位内联在数值后面
 *#############################################################################*/
void Refresh_Data(void)
{
    char buf[16];    /* 临时字符串缓冲区 */

    switch(disp_page)
    {
        case 0:   /*===== 第1页数据 =====*/
            /* 水温: "XX.X℃" */
            sprintf(buf, "%.1f", water_temp);
            OLED_ShowString(40, 0, (u8*)buf, 16);       /* 数值 8×16 */
            OLED_ShowCHinese(82, 0, 69);                 /* ℃符号 16×16 */

            /* 溶解氧: "XX.Xmg/L" */
            sprintf(buf, "%.1fmg/L", do_value);
            OLED_ShowString(56, 2, (u8*)buf, 16);

            /* pH值: "X.X" */
            sprintf(buf, "%.1f", ph_value);
            OLED_ShowString(24, 4, (u8*)buf, 16);
            break;

        case 1:   /*===== 第2页数据 =====*/
            /* 气温: "XX℃" */
            sprintf(buf, "%d", air_temp);
            OLED_ShowString(40, 0, (u8*)buf, 16);
            OLED_ShowCHinese(66, 0, 69);                 /* ℃ */

            /* 湿度: "XX%" */
            sprintf(buf, "%d%%", air_humi);
            OLED_ShowString(40, 2, (u8*)buf, 16);

            /* 气压: "XXX.XkPa" */
            sprintf(buf, "%.1fkPa", bmp_pressure);
            OLED_ShowString(40, 4, (u8*)buf, 16);

            /* 空气质量: "X.XX" */
            sprintf(buf, "%.2f", mq2_vol);
            OLED_ShowString(72, 6, (u8*)buf, 16);
            break;

        case 2:   /*===== 第3页数据 =====*/
            /* 气泵状态: 开(字库67) / 关(字库68) */
            if(aerator_status)
                OLED_ShowCHinese(56, 0, 67);    /* 开 */
            else
                OLED_ShowCHinese(56, 0, 68);    /* 关 */

            /* 水泵状态 */
            if(pump_status)
                OLED_ShowCHinese(40, 2, 67);    /* 开 */
            else
                OLED_ShowCHinese(40, 2, 68);    /* 关 */

            /* 风扇状态: "XX%" 或 关 */
            if(fan_adj > 0)
            {
                sprintf(buf, "%d%%", fan_adj);
                OLED_ShowString(40, 4, (u8*)buf, 16);
            }
            else
                OLED_ShowCHinese(40, 4, 68);    /* 关 */

            /* 控制模式: 自动 / 手动 */
            if(auto_mode)
            {
                OLED_ShowCHinese(40, 6, 64);    /* 自 */
                OLED_ShowCHinese(56, 6, 65);    /* 动 */
            }
            else
            {
                OLED_ShowCHinese(40, 6, 66);    /* 手 */
                OLED_ShowCHinese(56, 6, 65);    /* 动 */
            }
            break;

        default: break;
    }
}

/*#############################################################################
 *   AutoControl_Check — 自动控制逻辑 (仅在 auto_mode=1 时调用)
 *
 *   控制策略 (全部带滞回区间, 防止频繁开关):
 *   1. 气泵: DO < do_low_th(4.0) → 开; DO >= do_high_th(7.0) → 关
 *   2. 水泵: 水温 >= 30℃ → 开; 水温 < 28℃ → 关 
 *   3. 风扇: 42℃+ → 全速100%; 38℃+或烟雾超标 → 中速80%;
 *            <35℃且烟雾正常 → 关 
 *#############################################################################*/
void AutoControl_Check(void)
{
    /*========== 1. 气泵控制 (溶解氧驱动) ==========*/
    /* 亨利定律估算的DO随温度升高自然下降, 高温时额外增氧是合理的 */
    if(do_value < do_low_th && do_value > 0.1f)       /* DO过低 (且读数有效) */
    {
        Aerator_Set(AERATOR_ON);              /* GPIO PA9 = HIGH → MOS导通 → 气泵开 */
        aerator_status = 1;
    }
    else if(do_value >= do_high_th)                  /* DO达标  */
    {
        Aerator_Set(AERATOR_OFF);             /* GPIO PA9 = LOW → MOS截止 → 气泵关 */
        aerator_status = 0;
    }
   

    /*========== 2. 水泵控制 (水温驱动) ==========*/
    if(water_temp >= water_temp_high_th && water_temp < 80.0f)  /* 水温过高 */
    {
        Relay_Pump_Set(RELAY_PUMP_ON);         /* PA10 = LOW → 继电器吸合 → 水泵开 */
        pump_status = 1;
    }
    else if(water_temp < water_temp_high_th )          /* 水温回落  */
    {
        Relay_Pump_Set(RELAY_PUMP_OFF);        /* PA10 = HIGH → 继电器断开 → 水泵关 */
        pump_status = 0;
    }
   

    /*========== 3. 风扇控制 (气温 + 烟雾双条件, 多级调速) ==========*/
    if(air_temp >= AIR_TEMP_VERY_HIGH)                    /* 极端高温 42℃ → 全速 */
    {
        Fan_Set(FAN_ON);
        fan_adj = 100;
    }
    else if(air_temp >= air_temp_high_th || mq2_vol > smog_th)  /* 一般高温/烟雾 → 中速 */
    {
        Fan_Set(FAN_ON);
        fan_adj = 80;
    }
    else if(air_temp < air_temp_high_th  &&             /* 条件解除  */
            mq2_vol < smog_th )
    {
        Fan_Set(FAN_OFF);

        fan_adj = 0;
    }
   
}

/*#############################################################################
 *     Alarm_Check — 阈值报警检测 (4项异常 → 蜂鸣器 + alarm_flag=1)
 *
 *  检测项目:
 *  1. 溶解氧 < do_low_th → 蜂鸣器开
 *  2. pH值 < 6.5 或 pH > 8.5 → 蜂鸣器开
 *  3. 水温 ≤ 2℃(结冰风险) 或 ≥ 33℃ → 蜂鸣器开
 *  4. 烟雾浓度 > smog_th  → 蜂鸣器开
 *#############################################################################*/
void Alarm_Check(void)
{
    u8 should_alarm = 0;          /* 是否需要报警 */

    /* 溶解氧过低报警 (读数有效且低于阈值) */
    if(do_value < do_low_th && do_value > 0.1f)
        should_alarm = 1;

    /* pH异常报警 (低于低阈值 或 高于高阈值, 读数有效) */
    if((ph_value < ph_low_th && ph_value > 0.5f) ||
       (ph_value > ph_high_th && ph_value < 13.5f))
        should_alarm = 1;

    /* 水温异常报警 (过低=结冰风险, 过高=烫伤风险) */
    if(water_temp <= water_temp_low_th && water_temp > -50.0f)     /* 水温过低 */
        should_alarm = 1;
    if(water_temp >= water_temp_high_th && water_temp < 80.0f) /* 水温过高 */
        should_alarm = 1;

    /* 烟雾超标报警 */
    if(mq2_vol > smog_th)
        should_alarm = 1;

    /* 气温过高报警 */
    if(air_temp >= air_temp_high_th && air_temp > 0)
        should_alarm = 1;

    /* 执行报警/解除 */
    if(should_alarm)
    {
        Beep_Set(BEEP_ON);         /* PB9 输出 → 蜂鸣器响 */
        alarm_flag = 1;            /* alarm_flag 会被上报到OneNET → App显示红色脉冲 */
    }
    else
    {
        Beep_Set(BEEP_OFF);        /* 蜂鸣器关 */
        alarm_flag = 0;            /* 正常状态 */
    }
}




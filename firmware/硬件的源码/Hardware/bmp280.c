/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	bmp280.c
	*
	*	作者： 		金波胜
	*
	*	日期： 		2026-6-16
	*
	*	说明： 		PB1 - SCL, PB0 - SDA,地址 0x76
	*
	*************************************************************
	************************************************************
	************************************************************
**/
/**
 ************************************************************
 *  文件名  : bmp280.c
 *  功能    : BMP280 气压传感器驱动 (软件I2C)
 *  接口    : PB1 - SCL, PB0 - SDA, 地址 0x76
 *  说明    : 读取大气压(kPa)和温度(°C)
 *           用于溶解氧饱和度的压力补偿计算
 *           补偿算法依据Bosch BMP280数据手册
 ************************************************************
**/

#include "bmp280.h"
#include "delay.h"

/* ---- BMP280 I2C地址 ---- */
#define BMP280_ADDR      0x76    /* SDO接地时地址0x76 */
#define BMP280_ADDR_W    (BMP280_ADDR << 1)
#define BMP280_ADDR_R    ((BMP280_ADDR << 1) | 0x01)

/* ---- BMP280 寄存器 ---- */
#define BMP280_REG_ID           0xD0  /* 芯片ID, 应为0x58 */
#define BMP280_REG_RESET        0xE0  /* 复位, 写入0xB6 */
#define BMP280_REG_STATUS       0xF3  /* 状态 */
#define BMP280_REG_CTRL_MEAS    0xF4  /* 测量控制 */
#define BMP280_REG_CONFIG       0xF5  /* 配置 */
#define BMP280_REG_PRESS_MSB    0xF7  /* 气压 MSB */
#define BMP280_REG_PRESS_LSB    0xF8  /* 气压 LSB */
#define BMP280_REG_PRESS_XLSB   0xF9  /* 气压 XLSB */
#define BMP280_REG_TEMP_MSB     0xFA  /* 温度 MSB */
#define BMP280_REG_TEMP_LSB     0xFB  /* 温度 LSB */
#define BMP280_REG_TEMP_XLSB    0xFC  /* 温度 XLSB */
#define BMP280_REG_CALIB_FIRST  0x88  /* 校准数据起始 (温度) */
#define BMP280_REG_CALIB_LAST   0xA1  /* 校准数据结束 */
#define BMP280_CALIB_EXTRA_START 0xE1 /* 额外校准数据 */

/* ---- 校准系数 (全局, 上电读一次) ---- */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

int32_t t_fine;  /* 温度补偿中间变量, 给气压补偿用 */

/* ======================== 软件I2C基础函数 ======================== */

static void BMP280_I2C_Delay(void)
{
    DelayUs(5);  /* >4.7us 满足标准I2C时序 */
}

static void BMP280_I2C_Start(void)
{
    BMP280_SDA_H();
    BMP280_SCL_H();
    BMP280_I2C_Delay();
    BMP280_SDA_L();
    BMP280_I2C_Delay();
    BMP280_SCL_L();
}

static void BMP280_I2C_Stop(void)
{
    BMP280_SDA_L();
    BMP280_SCL_H();
    BMP280_I2C_Delay();
    BMP280_SDA_H();
    BMP280_I2C_Delay();
}

static uint8_t BMP280_I2C_WaitAck(void)
{
    uint8_t retry = 0;
    BMP280_SDA_H();
    BMP280_I2C_Delay();
    BMP280_SCL_H();
    BMP280_I2C_Delay();
    while(BMP280_SDA_IN())
    {
        if(++retry > 200) return 1;  /* 超时, 无应答 */
    }
    BMP280_SCL_L();
    return 0;
}

static void BMP280_I2C_SendAck(uint8_t ack)
{
    if(ack)
        BMP280_SDA_H();
    else
        BMP280_SDA_L();
    BMP280_I2C_Delay();
    BMP280_SCL_H();
    BMP280_I2C_Delay();
    BMP280_SCL_L();
    BMP280_SDA_H();
}

static void BMP280_I2C_WriteByte(uint8_t dat)
{
    uint8_t i;
    for(i = 0; i < 8; i++)
    {
        if(dat & 0x80)
            BMP280_SDA_H();
        else
            BMP280_SDA_L();
        dat <<= 1;
        BMP280_I2C_Delay();
        BMP280_SCL_H();
        BMP280_I2C_Delay();
        BMP280_SCL_L();
    }
}

static uint8_t BMP280_I2C_ReadByte(uint8_t ack)
{
    uint8_t i, dat = 0;
    BMP280_SDA_H();  /* 释放总线, 让从机驱动 */
    for(i = 0; i < 8; i++)
    {
        dat <<= 1;
        BMP280_SCL_H();
        BMP280_I2C_Delay();
        if(BMP280_SDA_IN()) dat |= 0x01;
        BMP280_SCL_L();
        BMP280_I2C_Delay();
    }
    BMP280_I2C_SendAck(ack);
    return dat;
}

/* ======================== BMP280 寄存器读写 ======================== */

static uint8_t BMP280_ReadReg(uint8_t reg)
{
    uint8_t val;
    BMP280_I2C_Start();
    BMP280_I2C_WriteByte(BMP280_ADDR_W);
    BMP280_I2C_WaitAck();
    BMP280_I2C_WriteByte(reg);
    BMP280_I2C_WaitAck();
    BMP280_I2C_Start();
    BMP280_I2C_WriteByte(BMP280_ADDR_R);
    BMP280_I2C_WaitAck();
    val = BMP280_I2C_ReadByte(0);  /* 读一字节, NACK结束 */
    BMP280_I2C_Stop();
    return val;
}

static void BMP280_WriteReg(uint8_t reg, uint8_t val)
{
    BMP280_I2C_Start();
    BMP280_I2C_WriteByte(BMP280_ADDR_W);
    BMP280_I2C_WaitAck();
    BMP280_I2C_WriteByte(reg);
    BMP280_I2C_WaitAck();
    BMP280_I2C_WriteByte(val);
    BMP280_I2C_WaitAck();
    BMP280_I2C_Stop();
}

static void BMP280_ReadMulti(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    BMP280_I2C_Start();
    BMP280_I2C_WriteByte(BMP280_ADDR_W);
    BMP280_I2C_WaitAck();
    BMP280_I2C_WriteByte(reg);
    BMP280_I2C_WaitAck();
    BMP280_I2C_Start();
    BMP280_I2C_WriteByte(BMP280_ADDR_R);
    BMP280_I2C_WaitAck();
    for(i = 0; i < len; i++)
    {
        if(i == len - 1)
            buf[i] = BMP280_I2C_ReadByte(0);  /* 最后一字节 NACK */
        else
            buf[i] = BMP280_I2C_ReadByte(1);  /* 中间字节 ACK */
    }
    BMP280_I2C_Stop();
}

/* ======================== 校准数据读取 ======================== */

static void BMP280_ReadCalib(void)
{
    uint8_t calib[26];  /* 0x88 ~ 0xA1 共26字节 */

    BMP280_ReadMulti(BMP280_REG_CALIB_FIRST, calib, 26)  /* 0x88~0xA1 */;

    /* 温度校准系数 */
    dig_T1 = (uint16_t)(calib[1]  << 8) | calib[0];
    dig_T2 = (int16_t)((calib[3]  << 8) | calib[2]);
    dig_T3 = (int16_t)((calib[5]  << 8) | calib[4]);

    /* 气压校准系数 */
    dig_P1 = (uint16_t)((calib[7]  << 8) | calib[6]);
    dig_P2 = (int16_t)((calib[9]  << 8) | calib[8]);
    dig_P3 = (int16_t)((calib[11] << 8) | calib[10]);
    dig_P4 = (int16_t)((calib[13] << 8) | calib[12]);
    dig_P5 = (int16_t)((calib[15] << 8) | calib[14]);
    dig_P6 = (int16_t)((calib[17] << 8) | calib[16]);
    dig_P7 = (int16_t)((calib[19] << 8) | calib[18]);
    dig_P8 = (int16_t)((calib[21] << 8) | calib[20]);
    dig_P9 = (int16_t)((calib[25] << 8) | calib[24])  /* 0xA0~0xA1 */;

    /* dig_P9 实际在 calib[24]~[25], 但标准BMP280 calib 0x88~0xA1 只到dig_P9
     * 读取0x88-0xA1共26字节已包含所有校准数据 */
}

/* ======================== 温度补偿(Bosch公式) ======================== */

static int32_t BMP280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) *
            ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
            ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;  /* 返回值单位: 0.01°C */
}

/* ======================== 气压补偿(Bosch公式) ======================== */

static uint32_t BMP280_CompensatePress(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
           ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if(var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)p;  /* 返回值单位: Pa */
}

/* ======================== 读取原始ADC值 ======================== */

static int32_t BMP280_ReadRawTemp(void)
{
    uint8_t buf[3];
    BMP280_ReadMulti(BMP280_REG_TEMP_MSB, buf, 3);
    return ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
}

int32_t BMP280_ReadRawPress(void)
{
    uint8_t buf[3];
    BMP280_ReadMulti(BMP280_REG_PRESS_MSB, buf, 3);
    return ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
}


/* ======================== 对外接口 ======================== */

/**
 * BMP280_Check: 检查芯片是否存在
 * 返回: 0=正常, 1=未检测到
 */
uint8_t BMP280_Check(void)
{
    uint8_t id = BMP280_ReadReg(BMP280_REG_ID);
    if(id == 0x58) return 0;
    return 1;
}

/**
 * BMP280_Read_Temperature: 读取BMP280温度
 * 返回: 温度 (°C)
 */
float BMP280_Read_Temperature(void)
{
    int32_t adc_T, temp_raw;
    adc_T = BMP280_ReadRawTemp();
    temp_raw = BMP280_CompensateTemp(adc_T);
    return (float)temp_raw / 100.0f;
}

/**
 * BMP280_Read_Pressure: 读取大气压
 * 返回: 气压 (kPa)
 */
float BMP280_Read_Pressure(void)
{
    int32_t adc_P;

    /* 读温度更新t_fine (不稳定则用固定值128000≈25°C) */
    adc_P = BMP280_ReadRawTemp();
    BMP280_CompensateTemp(adc_P);

    /* 再读气压 */
    adc_P = BMP280_ReadRawPress();
    return (float)BMP280_CompensatePress(adc_P) / 1000.0f;  /* Pa -> kPa */
}


/* ======================== 初始化 ======================== */

void BMP280_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    BMP280_SDA_H();
    BMP280_SCL_H();
    DelayMs(10);

    BMP280_ReadCalib();

    BMP280_WriteReg(BMP280_REG_CONFIG, 0x00);
    BMP280_WriteReg(BMP280_REG_CTRL_MEAS, 0x27);
    DelayMs(50);
}

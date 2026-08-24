/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	do_estimate.c
	*
	*	作者： 		金波胜
	*
	*	日期： 		2026-6-15
	* 		
	 *  功能    : 溶解氧估算 (基于水温和大气压)
	 *
	 *  原理    : 亨利定律 (Henry's Law)
	 *            水中溶解氧饱和浓度是水温和气压的函数
	 *
	 *  公式    : DO_sat(T) = 14.652 - 0.41022·T + 0.007991·T²
	 *                        - 0.000077774·T³
	 *            (淡水, 1atm = 101.325kPa 下的饱和值, 单位 mg/L)
	 *
	 *            压力修正:
	 *            DO_sat(T,P) = DO_sat(T) × (P / 101.325)
	 *
	 *  输入    : water_temp    — DS18B20水温 (°C)
	 *            pressure_kpa  — BMP280大气压 (kPa)
	 *
	 *  输出    : 溶解氧饱和浓度 (mg/L)
	 *
	 *  注意    : 此为理论饱和值, 非实测溶解氧!
	 *            实际值可能因藻类、鱼群密度、有机废物等显著低于饱和值
	 *            建议条件允许时升级为真正的溶解氧探头
	 ************************************************************
	**/

#include "do_estimate.h"

/**
 * DO_Estimate_Init: 溶解氧估算模块初始化
 * 说明: 无需初始化硬件, 此为纯计算模块
 */
void DO_Estimate_Init(void)
{
    /* 无硬件需要初始化 */
}

/**
 * DO_Estimate_Read: 估算溶解氧饱和浓度
 *
 * 参数:
 *   water_temp   — 水温 (°C), 来自DS18B20
 *   pressure_kpa — 大气压 (kPa), 来自BMP280
 *
 * 返回:
 *   溶解氧饱和浓度 (mg/L), 范围约 6~15
 *
 * 亨利定律多项式 (Grumd & Wunderlich, Freshwater):
 *   0°C → 14.6 mg/L
 *  10°C → 11.3 mg/L
 *  20°C →  9.1 mg/L
 *  25°C →  8.3 mg/L
 *  30°C →  7.6 mg/L
 *  35°C →  7.0 mg/L
 */
float DO_Estimate_Read(float water_temp, float pressure_kpa)
{
    float do_sat_std;   /* 标准大气压下饱和DO */
    float do_sat_final; /* 压力修正后的饱和DO */

    /* 防止异常值 */
    if(water_temp < -10.0f) water_temp = -10.0f;
    if(water_temp > 50.0f) water_temp = 50.0f;
    if(pressure_kpa < 80.0f)  pressure_kpa = 80.0f;   /* 极端低气压 */
    if(pressure_kpa > 110.0f) pressure_kpa = 110.0f;  /* 极端高气压 */

    /* 亨利定律: 标准大气压(101.325kPa)下的饱和DO多项式拟合
     * DO_sat = a0 + a1*T + a2*T² + a3*T³
     * 系数来源: Benson & Krause (1984), 淡水, 0 salinity
     */
    do_sat_std = 14.652f
               - 0.41022f * water_temp
               + 0.007991f * water_temp * water_temp
               - 0.000077774f * water_temp * water_temp * water_temp;

    if(do_sat_std < 0.0f) do_sat_std = 0.0f;

    /* 压力修正: 气压越低, 溶氧越少 */
    do_sat_final = do_sat_std * (pressure_kpa / 101.325f);

    return do_sat_final;
}

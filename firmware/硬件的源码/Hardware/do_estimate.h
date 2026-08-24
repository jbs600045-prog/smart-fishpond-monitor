#ifndef _DO_ESTIMATE_H_
#define _DO_ESTIMATE_H_

#include "stm32f10x.h"

/**
 * 溶解氧估算模块 (替代模拟量溶解氧传感器)
 *
 * 原理: 亨利定律 — 水中溶解氧饱和浓度由水温和气压决定
 *       DO_sat(T, P) = C*(T) × (P / 101.325)
 *
 * 输入: DS18B20水温(°C) + BMP280大气压(kPa)
 * 输出: 估算溶解氧饱和值(mg/L)
 *
 * 注意: 这是理论饱和值, 不是实际溶解氧浓度
 *       实际溶氧可能因生物耗氧、有机物分解等低于此值
 *       仅作为低成本替代方案使用
 */

void DO_Estimate_Init(void);
float DO_Estimate_Read(float water_temp, float pressure_kpa);

#endif

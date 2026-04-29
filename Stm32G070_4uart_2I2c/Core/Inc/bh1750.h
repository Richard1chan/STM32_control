/**
  ******************************************************************************
  * @file    bh1750.h
  * @brief   BH1750FVI 环境光传感器驱动
  *          I2C 接口，16bit 输出，量程 1~65535 lx
  *
  *  ADDR 引脚接 GND → 7位地址 0x23（HAL 写法 BH1750_ADDR_L）
  *  ADDR 引脚接 VCC → 7位地址 0x5C（HAL 写法 BH1750_ADDR_H）
  ******************************************************************************
  */

#ifndef __BH1750_H__
#define __BH1750_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"

/* ADDR 引脚决定从地址，HAL 需左移 1 位 */
#define BH1750_ADDR_L    (0x23u << 1)   /* ADDR = GND */
#define BH1750_ADDR_H    (0x5Cu << 1)   /* ADDR = VCC */

/**
 * @brief 上电并启动连续 H-Resolution 模式（1lx 分辨率，120ms 采样周期）
 *        内部调用 osDelay(180)，须在调度器启动后使用
 * @param hi2c   I2C 句柄
 * @param addr   从地址（BH1750_ADDR_L 或 BH1750_ADDR_H）
 * @return HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef BH1750_Init(I2C_HandleTypeDef *hi2c, uint16_t addr);

/**
 * @brief 读取当前光照度
 *        连续模式下每 120ms 自动更新，直接读取最新结果
 * @param hi2c   I2C 句柄
 * @param addr   从地址
 * @param lux    输出光照度（单位 lx）
 * @return HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef BH1750_Read(I2C_HandleTypeDef *hi2c, uint16_t addr, uint32_t *lux);

#ifdef __cplusplus
}
#endif

#endif /* __BH1750_H__ */

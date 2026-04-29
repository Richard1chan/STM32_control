/**
  ******************************************************************************
  * @file    aht20.h
  * @brief   AHT20-F 温湿度传感器驱动
  *          I2C 地址 0x38，标准 I2C 协议，供电 2.2-5.5V
  ******************************************************************************
  */

#ifndef __AHT20_H__
#define __AHT20_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"

/* AHT20 7位地址 0x38，HAL 需左移1位 */
#define AHT20_I2C_ADDR      (0x38u << 1)

typedef struct {
    float temperature;   /* 摄氏度 */
    float humidity;      /* %RH    */
} AHT20_Data_t;

/**
 * @brief 初始化 AHT20（上电等待 + 校准状态检查）
 *        必须在 FreeRTOS 调度器启动后调用（内部使用 osDelay）
 * @param hi2c  I2C 句柄
 * @return HAL_OK 成功，HAL_ERROR 失败
 */
HAL_StatusTypeDef AHT20_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief 触发一次测量并读取温湿度
 *        耗时约 80-130ms（内部使用 osDelay，不阻塞其他任务）
 * @param hi2c  I2C 句柄
 * @param data  输出数据
 * @return HAL_OK 成功，HAL_ERROR / HAL_BUSY 失败
 */
HAL_StatusTypeDef AHT20_Read(I2C_HandleTypeDef *hi2c, AHT20_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_H__ */

/**
  ******************************************************************************
  * @file    bh1750.c
  * @brief   BH1750FVI 环境光传感器驱动实现
  *
  * 通信时序（数据手册）：
  *   1. 发 Power On 命令（0x01）
  *   2. 发 Continuously H-Resolution Mode 命令（0x10）
  *   3. 等待 ≥180ms（最大测量时间）
  *   4. 读 2 字节原始值，lux = raw / 1.2
  ******************************************************************************
  */

#include "bh1750.h"
#include "cmsis_os.h"

/* 指令码（数据手册 Instruction Set Architecture） */
#define BH1750_CMD_POWER_ON     0x01u   /* 上电，等待测量命令      */
#define BH1750_CMD_CONT_H_RES   0x10u   /* 连续 H-Resolution 模式  */

HAL_StatusTypeDef BH1750_Init(I2C_HandleTypeDef *hi2c, uint16_t addr)
{
    uint8_t cmd;

    /* Power On */
    cmd = BH1750_CMD_POWER_ON;
    if (HAL_I2C_Master_Transmit(hi2c, addr, &cmd, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* 启动连续 H-Resolution 模式 */
    cmd = BH1750_CMD_CONT_H_RES;
    if (HAL_I2C_Master_Transmit(hi2c, addr, &cmd, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* 等待首次测量完成（最大 180ms） */
    osDelay(180);

    return HAL_OK;
}

HAL_StatusTypeDef BH1750_Read(I2C_HandleTypeDef *hi2c, uint16_t addr, uint32_t *lux)
{
    uint8_t raw[2];

    if (HAL_I2C_Master_Receive(hi2c, addr, raw, 2, 100) != HAL_OK)
        return HAL_ERROR;

    /* lux = (High<<8 | Low) / 1.2
     * 整数计算：先 ×10 再 /12，保留 0.1lx 精度后调用方截断为整数 */
    uint16_t count = ((uint16_t)raw[0] << 8) | raw[1];
    /* 避免浮点：*10/12 = *5/6，用 uint32 防溢出（最大 65535*5=327675） */
    *lux = ((uint32_t)count * 5u) / 6u;

    return HAL_OK;
}

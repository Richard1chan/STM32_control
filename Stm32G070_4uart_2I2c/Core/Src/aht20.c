/**
  ******************************************************************************
  * @file    aht20.c
  * @brief   AHT20-F 温湿度传感器驱动实现
  *
  * 通信时序（数据手册 §5.4）：
  *   1. 上电等待 ≥100ms
  *   2. 读状态字节，若 bit[3]=0（未校准）发初始化命令
  *   3. 发触发测量命令 0xAC 0x33 0x00
  *   4. 等待 ≥80ms
  *   5. 读 6 字节，检查 byte[0] bit[7] 忙标志
  *   6. 解析湿度（20bit）和温度（20bit）
  ******************************************************************************
  */

#include "aht20.h"
#include "cmsis_os.h"

/* -------------------------------------------------------------------------- */
/* 命令定义                                                                    */
/* -------------------------------------------------------------------------- */

#define AHT20_CMD_INIT        0xBEu   /* 初始化/校准命令      */
#define AHT20_CMD_MEASURE     0xACu   /* 触发测量命令         */
#define AHT20_CMD_SOFTRESET   0xBAu   /* 软复位命令           */

#define AHT20_STATUS_BUSY     0x80u   /* bit[7]: 1=忙         */
#define AHT20_STATUS_CALIB    0x08u   /* bit[3]: 1=已校准     */

#define AHT20_MEAS_TIMEOUT_MS   10u   /* 单次忙等重试间隔 ms  */
#define AHT20_MEAS_RETRIES      10u   /* 最大忙等重试次数     */

/* -------------------------------------------------------------------------- */
/* 公共函数                                                                    */
/* -------------------------------------------------------------------------- */

HAL_StatusTypeDef AHT20_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t status;
    uint8_t cmd[3];

    /* 上电稳定等待 ≥100ms */
    osDelay(100);

    /* 读状态字节 */
    if (HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDR, &status, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* 若未校准，发初始化命令 0xBE 0x08 0x00 */
    if ((status & AHT20_STATUS_CALIB) == 0) {
        cmd[0] = AHT20_CMD_INIT;
        cmd[1] = 0x08u;
        cmd[2] = 0x00u;
        if (HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR, cmd, 3, 100) != HAL_OK)
            return HAL_ERROR;
        osDelay(10);
    }

    return HAL_OK;
}

HAL_StatusTypeDef AHT20_Read(I2C_HandleTypeDef *hi2c, AHT20_Data_t *data)
{
    uint8_t cmd[3] = {AHT20_CMD_MEASURE, 0x33u, 0x00u};
    uint8_t raw[6];
    uint8_t retries = 0;

    /* 触发测量 */
    if (HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR, cmd, 3, 100) != HAL_OK)
        return HAL_ERROR;

    /* 等待 ≥80ms（传感器测量时间）*/
    osDelay(80);

    /* 轮询忙标志，最多等待 AHT20_MEAS_RETRIES × 10ms */
    do {
        if (HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDR, raw, 6, 100) != HAL_OK)
            return HAL_ERROR;
        if ((raw[0] & AHT20_STATUS_BUSY) == 0)
            break;
        osDelay(AHT20_MEAS_TIMEOUT_MS);
        retries++;
    } while (retries < AHT20_MEAS_RETRIES);

    if (raw[0] & AHT20_STATUS_BUSY)
        return HAL_BUSY;

    /* 解析湿度原始值（20bit）
     * byte[1][7:0] | byte[2][7:0] | byte[3][7:4]  */
    uint32_t hum_raw  = ((uint32_t)raw[1] << 12)
                      | ((uint32_t)raw[2] <<  4)
                      | ((uint32_t)raw[3] >>  4);

    /* 解析温度原始值（20bit）
     * byte[3][3:0] | byte[4][7:0] | byte[5][7:0]  */
    uint32_t temp_raw = (((uint32_t)raw[3] & 0x0Fu) << 16)
                      | ((uint32_t)raw[4] <<  8)
                      |  (uint32_t)raw[5];

    /* 转换公式（数据手册 §6）
     * RH[%] = (S_RH / 2^20) × 100
     * T[°C] = (S_T  / 2^20) × 200 - 50          */
    data->humidity    = (float)hum_raw  / 1048576.0f * 100.0f;
    data->temperature = (float)temp_raw / 1048576.0f * 200.0f - 50.0f;

    return HAL_OK;
}

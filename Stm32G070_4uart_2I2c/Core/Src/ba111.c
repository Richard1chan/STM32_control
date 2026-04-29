/**
 * @file    ba111.c
 * @brief   BA111 TDS/水温传感器芯片驱动
 */

#include "ba111.h"

/* 检测指令：命令(A0) + 参数(00 00 00 00) + 校验和(A0) */
static const uint8_t s_detect_cmd[BA111_FRAME_LEN] = {
    0xA0u, 0x00u, 0x00u, 0x00u, 0x00u, 0xA0u
};

HAL_StatusTypeDef BA111_Request(UART_HandleTypeDef *huart)
{
    return HAL_UART_Transmit(huart, (uint8_t *)s_detect_cmd, BA111_FRAME_LEN, 50u);
}

HAL_StatusTypeDef BA111_Parse(const uint8_t *buf, uint16_t len, BA111_Data_t *data)
{
    if (len < BA111_FRAME_LEN || buf[0] != BA111_RSP_HEADER) return HAL_ERROR;

    /* 校验和：前5字节累加取低8位 */
    uint8_t sum = 0u;
    for (uint8_t i = 0u; i < 5u; i++) sum += buf[i];
    if (sum != buf[5]) return HAL_ERROR;

    data->tds_ppm = ((uint16_t)buf[1] << 8u) | buf[2];

    /* 原始温度为 ×100（如 0x0A96=2710 代表 27.10°C），转为 ×10（271）*/
    uint16_t raw_temp = ((uint16_t)buf[3] << 8u) | buf[4];
    data->temp10 = (int16_t)(raw_temp / 10u);

    return HAL_OK;
}

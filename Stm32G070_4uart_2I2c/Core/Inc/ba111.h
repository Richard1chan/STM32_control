/**
 * @file    ba111.h
 * @brief   BA111 TDS/水温传感器芯片驱动（Atombit）
 *          UART 协议：9600 bps, 8N1，无校验
 *          采用主动查询模式：MCU 发送检测指令，BA111 返回 6 字节响应
 *          校验和：前5字节逐字节累加取低8位（& 0xFF）
 *
 *          仅用于 BOT 板（UART4，PA0=TX / PA1=RX）
 */

#ifndef __BA111_H__
#define __BA111_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BA111_FRAME_LEN     6u      /* 请求帧和响应帧均为 6 字节 */
#define BA111_RSP_HEADER    0xAAu   /* 检测响应帧头 */

typedef struct {
    uint16_t tds_ppm;  /* TDS 值，单位 ppm，范围 0~3000 */
    int16_t  temp10;   /* 水温 × 10，单位 0.1°C（如 271 = 27.1°C） */
} BA111_Data_t;

/**
 * @brief 向 BA111 发送检测指令 (A0 00 00 00 00 A0)
 * @param huart  UART 句柄（UART4）
 * @return HAL_OK / HAL_ERROR
 */
HAL_StatusTypeDef BA111_Request(UART_HandleTypeDef *huart);

/**
 * @brief 解析 BA111 检测响应帧
 * @param buf   接收缓冲区
 * @param len   实际接收字节数
 * @param data  解析结果输出
 * @return HAL_OK：解析成功；HAL_ERROR：帧头错误或校验失败
 */
HAL_StatusTypeDef BA111_Parse(const uint8_t *buf, uint16_t len, BA111_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __BA111_H__ */

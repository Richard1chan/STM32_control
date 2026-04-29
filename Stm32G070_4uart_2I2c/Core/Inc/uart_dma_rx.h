/**
  ******************************************************************************
  * @file    uart_dma_rx.h
  * @brief   DMA circular buffer + IDLE interrupt UART receive driver for
  *          USART1-4 on STM32G071.
  *
  * 用法：
  *   1. main.c USER CODE BEGIN 2 中调用 UART_DMA_RX_Init()
  *   2. FreeRTOS 任务中调用 UART_DMA_RX_Read() 取数据
  *   3. 可选：调用 UART_DMA_RX_Available() 判断是否有数据
  ******************************************************************************
  */

#ifndef __UART_DMA_RX_H__
#define __UART_DMA_RX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* DMA 循环缓冲区大小（必须为 2 的幂，且 <= 65535） */
#define UART_DMA_BUF_SIZE   256u
/* 应用层环形缓冲区大小 */
#define UART_RX_BUF_SIZE    512u

/**
 * @brief 启动 4 路 UART 的 DMA 循环接收并使能 IDLE 中断
 *        在所有 MX_USARTx_UART_Init() 完成后、osKernelStart() 之前调用
 */
void     UART_DMA_RX_Init(void);

/**
 * @brief 注册接收任务句柄，IDLE 中断触发时通过 Task Notification 唤醒该任务
 *        在任务创建完成后（MX_FREERTOS_Init 内）调用，osKernelStart() 之前
 * @param huart      目标 UART 句柄
 * @param taskHandle 对应的 FreeRTOS 任务句柄（TaskHandle_t / osThreadId_t）
 */
void     UART_DMA_RX_RegisterTask(UART_HandleTypeDef *huart, TaskHandle_t taskHandle);

/**
 * @brief 从指定 UART 的应用层环形缓冲区读取数据
 * @param huart   目标 UART 句柄（&huart1 ~ &huart4）
 * @param buf     接收缓冲区
 * @param max_len 最多读取字节数
 * @return 实际读取字节数
 */
uint16_t UART_DMA_RX_Read(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t max_len);

/**
 * @brief 返回指定 UART 应用层缓冲区中可读字节数
 */
uint16_t UART_DMA_RX_Available(UART_HandleTypeDef *huart);

/**
 * @brief UART IDLE 中断处理：搬移 DMA 新增数据并通知对应任务
 *        在对应 USARTx_IRQHandler 中调用
 */
void     UART_DMA_RX_IdleHandler(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __UART_DMA_RX_H__ */

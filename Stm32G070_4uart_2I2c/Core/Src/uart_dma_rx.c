/**
  ******************************************************************************
  * @file    uart_dma_rx.c
  * @brief   DMA circular buffer + IDLE interrupt UART receive driver
  *
  * 原理：
  *   - 每路 UART 启动一个 DMA 循环接收（Circular 模式），DMA 不停地写入
  *     dma_buf[UART_DMA_BUF_SIZE]，无需 CPU 干预。
  *   - 每当 UART 总线空闲（IDLE 标志），触发 UART 中断，将 DMA 缓冲区中
  *     自上次读取位置起新写入的字节搬入应用层 rx_buf 环形缓冲区。
  *   - FreeRTOS 任务调用 UART_DMA_RX_Read() 从 rx_buf 取数据。
  *
  * 线程安全：
  *   ISR 只写 rx_wr，任务只写 rx_rd，符合 SPSC 无锁队列约束。
  ******************************************************************************
  */

#include "uart_dma_rx.h"
#include "usart.h"

/* -------------------------------------------------------------------------- */
/* 内部数据结构                                                                */
/* -------------------------------------------------------------------------- */

typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t  dma_buf[UART_DMA_BUF_SIZE];   /* DMA 直接写入，Circular 模式   */
    uint8_t  rx_buf[UART_RX_BUF_SIZE];     /* 应用层读取的环形缓冲区         */
    volatile uint16_t rx_wr;               /* 写指针（ISR 更新）             */
    uint16_t rx_rd;                        /* 读指针（任务更新）             */
    uint16_t dma_prev;                     /* 上次处理的 DMA 写位置          */
    TaskHandle_t task_handle;              /* 注册的接收任务句柄             */
} UART_RX_Ctx;

static UART_RX_Ctx s_ctx[4];

/* -------------------------------------------------------------------------- */
/* 内部辅助函数                                                                */
/* -------------------------------------------------------------------------- */

static UART_RX_Ctx *find_ctx(UART_HandleTypeDef *huart)
{
    for (int i = 0; i < 4; i++) {
        if (s_ctx[i].huart == huart)
            return &s_ctx[i];
    }
    return NULL;
}

/* 将 src_buf 中 [start, end) 的字节追加到 ctx 的 rx_buf 环形缓冲区
   调用者保证 (end - start) 不超过 UART_DX_BUF_SIZE，且 start <= end */
static void push_bytes(UART_RX_Ctx *c, const uint8_t *src, uint16_t start, uint16_t end)
{
    for (uint16_t i = start; i < end; i++) {
        c->rx_buf[c->rx_wr] = src[i];
        c->rx_wr = (c->rx_wr + 1u) % UART_RX_BUF_SIZE;
    }
}

/* -------------------------------------------------------------------------- */
/* 公共 API                                                                   */
/* -------------------------------------------------------------------------- */

void UART_DMA_RX_Init(void)
{
    s_ctx[0].huart = &huart1;
    s_ctx[1].huart = &huart2;
    s_ctx[2].huart = &huart3;
    s_ctx[3].huart = &huart4;

    for (int i = 0; i < 4; i++) {
        s_ctx[i].rx_wr      = 0;
        s_ctx[i].rx_rd      = 0;
        s_ctx[i].dma_prev   = 0;
        s_ctx[i].task_handle = NULL;
        HAL_UART_Receive_DMA(s_ctx[i].huart, s_ctx[i].dma_buf, UART_DMA_BUF_SIZE);
        __HAL_UART_ENABLE_IT(s_ctx[i].huart, UART_IT_IDLE);
    }

    /* 使能 UART IDLE 中断对应的 NVIC 向量，优先级与 DMA 一致（3） */
    HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    /* USART3 和 USART4 共享同一 IRQ 向量 */
    HAL_NVIC_SetPriority(USART3_4_LPUART1_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART3_4_LPUART1_IRQn);
}

void UART_DMA_RX_RegisterTask(UART_HandleTypeDef *huart, TaskHandle_t taskHandle)
{
    UART_RX_Ctx *c = find_ctx(huart);
    if (c) c->task_handle = taskHandle;
}

void UART_DMA_RX_IdleHandler(UART_HandleTypeDef *huart)
{
    UART_RX_Ctx *c = find_ctx(huart);
    if (!c) return;

    /* 当前 DMA 写位置 = 缓冲区大小 - DMA 剩余计数 */
    uint16_t dma_pos = (uint16_t)(UART_DMA_BUF_SIZE -
                                  (uint16_t)huart->hdmarx->Instance->CNDTR);

    if (dma_pos == c->dma_prev) return;

    if (dma_pos > c->dma_prev) {
        push_bytes(c, c->dma_buf, c->dma_prev, dma_pos);
    } else {
        /* DMA 回绕：先处理尾部，再处理头部 */
        push_bytes(c, c->dma_buf, c->dma_prev, UART_DMA_BUF_SIZE);
        push_bytes(c, c->dma_buf, 0,           dma_pos);
    }

    c->dma_prev = dma_pos;

    /* 唤醒对应的接收任务 */
    if (c->task_handle != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(c->task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

uint16_t UART_DMA_RX_Available(UART_HandleTypeDef *huart)
{
    UART_RX_Ctx *c = find_ctx(huart);
    if (!c) return 0;
    return (uint16_t)((c->rx_wr - c->rx_rd + UART_RX_BUF_SIZE) % UART_RX_BUF_SIZE);
}

uint16_t UART_DMA_RX_Read(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t max_len)
{
    UART_RX_Ctx *c = find_ctx(huart);
    if (!c) return 0;

    uint16_t avail = (uint16_t)((c->rx_wr - c->rx_rd + UART_RX_BUF_SIZE) % UART_RX_BUF_SIZE);
    uint16_t n     = (avail < max_len) ? avail : max_len;

    for (uint16_t i = 0; i < n; i++) {
        buf[i]   = c->rx_buf[c->rx_rd];
        c->rx_rd = (c->rx_rd + 1u) % UART_RX_BUF_SIZE;
    }
    return n;
}

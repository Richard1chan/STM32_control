/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define SLAVE_A_ID 0x19
#define SLAVE_B_ID 0x1A

#define SLAVE_ID  SLAVE_A_ID

/* Solenoid (电磁�?) */
#define SOLENOID0_PORT      GPIOA
#define SOLENOID0_PIN       GPIO_PIN_3

#define SOLENOID1_PORT      GPIOA
#define SOLENOID1_PIN       GPIO_PIN_2

#define SOLENOID2_PORT      GPIOA
#define SOLENOID2_PIN       GPIO_PIN_5

#define SOLENOID3_PORT      GPIOA
#define SOLENOID3_PIN       GPIO_PIN_6

/* Water Pump (水泵) */
#define WATER_PUMP1_PORT    GPIOA
#define WATER_PUMP1_PIN     GPIO_PIN_7

#define WATER_PUMP2_PORT    GPIOB
#define WATER_PUMP2_PIN     GPIO_PIN_0

/* Micro Pump (蠕动�?) */
#define MICRO_PUMP0_PORT    GPIOB
#define MICRO_PUMP0_PIN     GPIO_PIN_1

#define MICRO_PUMP1_PORT    GPIOB
#define MICRO_PUMP1_PIN     GPIO_PIN_2

#define MICRO_PUMP2_PORT    GPIOB
#define MICRO_PUMP2_PIN     GPIO_PIN_10

#define MICRO_PUMP3_PORT    GPIOB
#define MICRO_PUMP3_PIN     GPIO_PIN_11

/* FAN (风扇) */
#define FAN0_FAN1_PORT      GPIOA
#define FAN0_FAN1_PIN       GPIO_PIN_4

/* Door (门磁) */
#define DOOR_PORT           GPIOB
#define DOOR_PIN            GPIO_PIN_12

/* Pallet (托盘) */
#define PALLET0_PORT        GPIOB
#define PALLET0_PIN         GPIO_PIN_15

#define PALLET1_PORT        GPIOC
#define PALLET1_PIN         GPIO_PIN_6

#define PALLET2_PORT        GPIOB
#define PALLET2_PIN         GPIO_PIN_14

#define PALLET3_PORT        GPIOB
#define PALLET3_PIN         GPIO_PIN_13
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static volatile uint8_t rx_buf4;

static osMessageQueueId_t modbusRxQueue;
static osSemaphoreId_t    modbusIdleSem;

static uint32_t stat_frames     = 0;
static uint32_t stat_crc_err    = 0;
static uint32_t stat_tx_err     = 0;
static uint32_t stat_uart4_errs = 0;

/* 由 HAL_UART_ErrorCallback（ISR）置位，任务上下文重启 DMA */
static volatile uint8_t  uart4_need_restart    = 0;
/* ISR 保存 HAL 错误码，任务打印时解码（PE/NE/FE/ORE/DMA）*/
static volatile uint32_t uart4_last_error_code = 0;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void     debug_printf(const char *fmt, ...);
static void     debug_dump_hex(const char *tag, const uint8_t *buf, uint16_t len);
void            Modbus_IdleCallback(void);          /* called from ISR */
static uint16_t Modbus_CRC16(uint8_t *puchMsg, uint16_t usDataLen);
static uint16_t ReadModbusStatus(void);
static void     WriteModbusStatus(uint16_t val);
static void     SendModbusException(uint8_t fc, uint8_t exc);
static void     ProcessModbusFrame(uint8_t *frame, uint16_t len);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

  /* 创建 Modbus 接收队列：32字节，足够容纳115200波特率下帧间切换的积压 */
  modbusRxQueue = osMessageQueueNew(32, sizeof(uint8_t), NULL);
  /* 创建帧结束信号量（二值，初始为0），由 UART4 IDLE 中断释放 */
  modbusIdleSem = osSemaphoreNew(1, 0, NULL);

  /* 启动 DMA 接收，再开启 IDLE 中断 */
  HAL_UART_Receive_DMA(&huart4, (uint8_t *)&rx_buf4, 1);
  __HAL_UART_ENABLE_IT(&huart4, UART_IT_IDLE);

  debug_printf("\r\n[BOOT] Modbus slave 0x%02X ready (UART4 115200 8N1)\r\n", SLAVE_ID);

  uint8_t  rx_frame[128];
  uint16_t rx_len = 0;

  /* Infinite loop */
  for(;;)
  {
    /*
     * 等待 IDLE 信号（硬件帧尾检测，<1ms）或 3ms 超时兜底。
     * 3ms >> 3.5字符时间@115200(~305us)，保证帧间隔判断可靠。
     */
    osSemaphoreAcquire(modbusIdleSem, 3);

    /* 总线设备重启等导致的 UART 帧错误/过载错误恢复 */
    if (uart4_need_restart) {
      uart4_need_restart = 0;
      uint32_t ec = uart4_last_error_code;
      rx_len = 0;
      osMessageQueueReset(modbusRxQueue);
      HAL_UART_Receive_DMA(&huart4, (uint8_t *)&rx_buf4, 1);
      __HAL_UART_ENABLE_IT(&huart4, UART_IT_IDLE);
      debug_printf("[UART4] ERR recovered #%lu: code=0x%02lX%s%s%s%s%s\r\n",
                   stat_uart4_errs, ec,
                   (ec & HAL_UART_ERROR_PE)  ? " PE"  : "",
                   (ec & HAL_UART_ERROR_NE)  ? " NE"  : "",
                   (ec & HAL_UART_ERROR_FE)  ? " FE"  : "",
                   (ec & HAL_UART_ERROR_ORE) ? " ORE" : "",
                   (ec & HAL_UART_ERROR_DMA) ? " DMA" : "");
      continue;
    }

    /* 排空队列，把所有已到达字节收入帧缓冲 */
    uint8_t byte;
    while (osMessageQueueGet(modbusRxQueue, &byte, NULL, 0) == osOK) {
      if (rx_len < sizeof(rx_frame)) {
        rx_frame[rx_len++] = byte;
      }
    }

    if (rx_len == 0) continue;

    if (rx_len >= 8) {
      ProcessModbusFrame(rx_frame, rx_len);
    } else {
      debug_printf("[MODBUS] Short frame %u bytes, discarded\r\n", rx_len);
      debug_dump_hex("[SHORT]", rx_frame, rx_len);
    }
    rx_len = 0;
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* ------------------- Debug (UART1) ------------------- */
static void debug_printf(const char *fmt, ...)
{
  static char buf[128];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len > 0) {
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 100);
  }
}

/* ------------------- 十六进制帧转储（仅任务上下文）------------------- */
static void debug_dump_hex(const char *tag, const uint8_t *buf, uint16_t len)
{
  char line[96];
  int pos = snprintf(line, sizeof(line), "%s[%u]:", tag, len);
  uint16_t show = (len > 16) ? 16 : len;
  for (uint16_t i = 0; i < show; i++) {
    pos += snprintf(line + pos, sizeof(line) - pos, " %02X", buf[i]);
  }
  if (len > 16) {
    pos += snprintf(line + pos, sizeof(line) - pos, " ..");
  }
  pos += snprintf(line + pos, sizeof(line) - pos, "\r\n");
  HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)pos, 200);
}

/* ------------------- IDLE 中断回调（从 stm32g0xx_it.c 调用）------------------- */
/* 在 ISR 中释放信号量，通知任务一帧已结束 */
void Modbus_IdleCallback(void)
{
  if (modbusIdleSem != NULL) {
    osSemaphoreRelease(modbusIdleSem);
  }
}

/* ------------------- DMA 字节接收回调 ------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART4) {
    if (modbusRxQueue != NULL) {
      osMessageQueuePut(modbusRxQueue, (const void *)&rx_buf4, 0, 0);
    }
  }
}

/* ------------------- UART 错误回调（ISR 上下文）------------------- */
/* 总线上其他设备重启/掉电会产生帧错误(FE)或过载(ORE)，HAL 因此中止 DMA。
 * 这里只置标志并唤醒任务，实际重启在任务上下文中执行，避免在 ISR 里调用 HAL。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART4) {
    stat_uart4_errs++;
    uart4_last_error_code = huart->ErrorCode;
    uart4_need_restart = 1;
    if (modbusIdleSem != NULL) {
      osSemaphoreRelease(modbusIdleSem);
    }
  }
}

/* ------------------- GPIO 读写 ------------------- */
static uint16_t ReadModbusStatus(void)
{
  uint16_t s = 0;
  if (HAL_GPIO_ReadPin(WATER_PUMP1_PORT, WATER_PUMP1_PIN) == GPIO_PIN_SET) s |= (1 << 0);
  if (HAL_GPIO_ReadPin(WATER_PUMP2_PORT, WATER_PUMP2_PIN) == GPIO_PIN_SET) s |= (1 << 1);
  if (HAL_GPIO_ReadPin(MICRO_PUMP0_PORT, MICRO_PUMP0_PIN) == GPIO_PIN_SET) s |= (1 << 2);
  if (HAL_GPIO_ReadPin(MICRO_PUMP1_PORT, MICRO_PUMP1_PIN) == GPIO_PIN_SET) s |= (1 << 3);
  if (HAL_GPIO_ReadPin(MICRO_PUMP2_PORT, MICRO_PUMP2_PIN) == GPIO_PIN_SET) s |= (1 << 4);
  if (HAL_GPIO_ReadPin(MICRO_PUMP3_PORT, MICRO_PUMP3_PIN) == GPIO_PIN_SET) s |= (1 << 5);
  if (HAL_GPIO_ReadPin(SOLENOID0_PORT,   SOLENOID0_PIN)   == GPIO_PIN_SET) s |= (1 << 6);
  if (HAL_GPIO_ReadPin(SOLENOID1_PORT,   SOLENOID1_PIN)   == GPIO_PIN_SET) s |= (1 << 7);
  if (HAL_GPIO_ReadPin(SOLENOID2_PORT,   SOLENOID2_PIN)   == GPIO_PIN_SET) s |= (1 << 8);
  if (HAL_GPIO_ReadPin(SOLENOID3_PORT,   SOLENOID3_PIN)   == GPIO_PIN_SET) s |= (1 << 9);
  if (HAL_GPIO_ReadPin(FAN0_FAN1_PORT,   FAN0_FAN1_PIN)   == GPIO_PIN_SET) s |= (1 << 10);
  /* 输入 */
  if (HAL_GPIO_ReadPin(DOOR_PORT,    DOOR_PIN)    == GPIO_PIN_SET) s |= (1 << 11);
  if (HAL_GPIO_ReadPin(PALLET0_PORT, PALLET0_PIN) == GPIO_PIN_SET) s |= (1 << 12);
  if (HAL_GPIO_ReadPin(PALLET1_PORT, PALLET1_PIN) == GPIO_PIN_SET) s |= (1 << 13);
  if (HAL_GPIO_ReadPin(PALLET2_PORT, PALLET2_PIN) == GPIO_PIN_SET) s |= (1 << 14);
  if (HAL_GPIO_ReadPin(PALLET3_PORT, PALLET3_PIN) == GPIO_PIN_SET) s |= (1 << 15);
  return s;
}

static void WriteModbusStatus(uint16_t val)
{
  HAL_GPIO_WritePin(WATER_PUMP1_PORT, WATER_PUMP1_PIN, (val & (1<<0))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(WATER_PUMP2_PORT, WATER_PUMP2_PIN, (val & (1<<1))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP0_PORT, MICRO_PUMP0_PIN, (val & (1<<2))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP1_PORT, MICRO_PUMP1_PIN, (val & (1<<3))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP2_PORT, MICRO_PUMP2_PIN, (val & (1<<4))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP3_PORT, MICRO_PUMP3_PIN, (val & (1<<5))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID0_PORT,   SOLENOID0_PIN,   (val & (1<<6))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID1_PORT,   SOLENOID1_PIN,   (val & (1<<7))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID2_PORT,   SOLENOID2_PIN,   (val & (1<<8))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID3_PORT,   SOLENOID3_PIN,   (val & (1<<9))  ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(FAN0_FAN1_PORT,   FAN0_FAN1_PIN,   (val & (1<<10)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ------------------- CRC16 ------------------- */
static uint16_t Modbus_CRC16(uint8_t *puchMsg, uint16_t usDataLen)
{
  uint16_t wCRCin = 0xFFFF;
  while (usDataLen--) {
    wCRCin ^= *puchMsg++;
    for (int i = 0; i < 8; i++) {
      if (wCRCin & 0x01) { wCRCin = (wCRCin >> 1) ^ 0xA001; }
      else               { wCRCin >>= 1; }
    }
  }
  return wCRCin;
}

/* ------------------- Modbus 异常应答 ------------------- */
static void SendModbusException(uint8_t fc, uint8_t exc)
{
  uint8_t resp[5];
  resp[0] = SLAVE_ID;
  resp[1] = fc | 0x80;
  resp[2] = exc;
  uint16_t crc = Modbus_CRC16(resp, 3);
  resp[3] = crc & 0xFF;
  resp[4] = (crc >> 8) & 0xFF;
  HAL_UART_Transmit(&huart4, resp, 5, 50);
  debug_printf("[MODBUS] Exception FC=0x%02X exc=0x%02X\r\n", fc, exc);
}

/* ------------------- Modbus 帧解析与响应 ------------------- */
static void ProcessModbusFrame(uint8_t *frame, uint16_t len)
{
  stat_frames++;

  /* 1. 从机地址过滤（广播地址 0x00 不回应）*/
  if (frame[0] != SLAVE_ID) return;

  /* 地址匹配：先打印原始帧，便于对比 CRC 和功能码 */
  debug_dump_hex("[RX]", frame, len);

  /* 2. CRC 校验 */
  uint16_t calc_crc = Modbus_CRC16(frame, len - 2);
  uint16_t recv_crc = (uint16_t)(frame[len-2]) | ((uint16_t)(frame[len-1]) << 8);
  if (calc_crc != recv_crc) {
    stat_crc_err++;
    debug_printf("[MODBUS] CRC ERR frame#%lu: got=0x%04X calc=0x%04X len=%u\r\n",
                 stat_frames, recv_crc, calc_crc, len);
    return;
  }

  uint8_t fc = frame[1];

  /* 3. FC 0x06: Write Single Register */
  if (fc == 0x06) {
    uint16_t reg_addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t data     = ((uint16_t)frame[4] << 8) | frame[5];

    if (reg_addr != 0x0000) {
      debug_printf("[MODBUS] FC06 illegal addr=0x%04X\r\n", reg_addr);
      SendModbusException(fc, 0x02); /* Illegal Data Address */
      return;
    }

    WriteModbusStatus(data);

    /* 回显请求帧（Modbus FC06 标准应答） */
    uint8_t resp[8];
    memcpy(resp, frame, 6);
    uint16_t resp_crc = Modbus_CRC16(resp, 6);
    resp[6] = resp_crc & 0xFF;
    resp[7] = (resp_crc >> 8) & 0xFF;
    if (HAL_UART_Transmit(&huart4, resp, 8, 50) != HAL_OK) {
      stat_tx_err++;
      debug_printf("[MODBUS] TX ERR (FC06) total_tx_err=%lu\r\n", stat_tx_err);
    } else {
      debug_printf("[MODBUS] FC06 write 0x%04X ok (frame#%lu)\r\n", data, stat_frames);
      debug_dump_hex("[TX]", resp, 8);
    }
  }
  /* 4. FC 0x04: Read Input Registers */
  else if (fc == 0x04) {
    uint16_t reg_addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t reg_qty  = ((uint16_t)frame[4] << 8) | frame[5];

    if (reg_addr != 0x0000 || reg_qty != 1) {
      debug_printf("[MODBUS] FC04 illegal addr=0x%04X qty=%u\r\n", reg_addr, reg_qty);
      SendModbusException(fc, 0x02); /* Illegal Data Address */
      return;
    }

    uint16_t data = ReadModbusStatus();

    uint8_t resp[7];
    resp[0] = SLAVE_ID;
    resp[1] = 0x04;
    resp[2] = 0x02;
    resp[3] = (data >> 8) & 0xFF;
    resp[4] = data & 0xFF;
    uint16_t resp_crc = Modbus_CRC16(resp, 5);
    resp[5] = resp_crc & 0xFF;
    resp[6] = (resp_crc >> 8) & 0xFF;
    if (HAL_UART_Transmit(&huart4, resp, 7, 50) != HAL_OK) {
      stat_tx_err++;
      debug_printf("[MODBUS] TX ERR (FC04) total_tx_err=%lu\r\n", stat_tx_err);
    } else {
      debug_printf("[MODBUS] FC04 read 0x%04X ok (frame#%lu)\r\n", data, stat_frames);
      debug_dump_hex("[TX]", resp, 7);
    }
  }
  /* 5. 不支持的功能码：返回异常 0x01 Illegal Function */
  else {
    debug_printf("[MODBUS] Illegal FC=0x%02X (frame#%lu)\r\n", fc, stat_frames);
    SendModbusException(fc, 0x01);
  }
}

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

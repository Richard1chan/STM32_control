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
#include "uart_dma_rx.h"
#include "i2c.h"
#include "aht20.h"
#include "bh1750.h"
#include "sc4m01a.h"
#include "modbus_rtu.h"
#include "ba111.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t uart1TaskHandle;
const osThreadAttr_t uart1Task_attributes = {
  .name = "uart1Task",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

osThreadId_t uart2TaskHandle;
const osThreadAttr_t uart2Task_attributes = {
  .name = "uart2Task",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

osThreadId_t uart3TaskHandle;
const osThreadAttr_t uart3Task_attributes = {
  .name = "uart3Task",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

osThreadId_t uart4TaskHandle;
const osThreadAttr_t uart4Task_attributes = {
  .name = "uart4Task",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

osThreadId_t aht20TaskHandle;
const osThreadAttr_t aht20Task_attributes = {
  .name = "aht20Task",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
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
void StartUart1Task(void *argument);
void StartUart2Task(void *argument);
void StartUart3Task(void *argument);
void StartUart4Task(void *argument);
void StartAht20Task(void *argument);
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
  uart1TaskHandle = osThreadNew(StartUart1Task, NULL, &uart1Task_attributes);
  uart2TaskHandle = osThreadNew(StartUart2Task, NULL, &uart2Task_attributes);
  uart3TaskHandle = osThreadNew(StartUart3Task, NULL, &uart3Task_attributes);
  uart4TaskHandle = osThreadNew(StartUart4Task, NULL, &uart4Task_attributes);

  /* 将任务句柄注册到驱动，IDLE 中断触发时唤醒对应任�? */
  UART_DMA_RX_RegisterTask(&huart1, uart1TaskHandle);
  UART_DMA_RX_RegisterTask(&huart2, uart2TaskHandle);
  UART_DMA_RX_RegisterTask(&huart3, uart3TaskHandle);
  UART_DMA_RX_RegisterTask(&huart4, uart4TaskHandle);

  aht20TaskHandle = osThreadNew(StartAht20Task, NULL, &aht20Task_attributes);
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
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief USART1 接收任务
 *        �? IDLE 中断通过 Task Notification 唤醒�?10ms 超时兜底轮询
 */
void StartUart1Task(void *argument)
{
    uint8_t buf[256];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

        uint16_t n;
        while ((n = UART_DMA_RX_Read(&huart1, buf, sizeof(buf))) > 0) {
            /* USER CODE BEGIN Uart1Task_Process */
            /* 在此处理 huart1 收到�? n 字节数据，数据位�? buf[0..n-1] */
            /* USER CODE END Uart1Task_Process */
        }
    }
}

/**
 * @brief USART2 Modbus RTU 从站任务（RS485，从机地�? 0x01�?
 *        DMA+IDLE 接收帧，调用 Modbus_Process 解析并回�?
 */
void StartUart2Task(void *argument)
{
    uint8_t buf[256];
    Modbus_Init();

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

        uint16_t n;
        while ((n = UART_DMA_RX_Read(&huart2, buf, sizeof(buf))) > 0) {
            Modbus_Process(buf, n);
        }
    }
}

/**
 * @brief SC-4M01A 空气质量采集任务（USART3�?9600 bps�?
 *        持续接收模块每秒上报�? 14 字节帧，�? 10 秒�?�过 UART1 打印�?新数�?
 */
void StartUart3Task(void *argument)
{
    uint8_t          raw[64];
    uint16_t         n;
    SC4M01A_Parser_t parser;
    SC4M01A_Data_t   latest  = {0};
    uint8_t          has_data = 0;
    char             out[128];
    int              len;
    TickType_t       last_print = xTaskGetTickCount();

    SC4M01A_ParserInit(&parser);

    for (;;) {
        /* 等待 IDLE 中断通知�?10ms 超时兜底 */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

        /* 取出�?有可用字节，喂给解析�? */
        while ((n = UART_DMA_RX_Read(&huart3, raw, sizeof(raw))) > 0) {
            if (SC4M01A_Feed(&parser, raw, n, &latest)) {
                has_data = 1;
                g_modbus_input[MODBUS_REG_CO2_1] = (int16_t)latest.co2;
            }
        }

        /* �? 10 秒打印一�? */
        if ((xTaskGetTickCount() - last_print) >= pdMS_TO_TICKS(10000)) {
            last_print = xTaskGetTickCount();

            if (has_data) {
                /* 温度可为负�?�，单独处理符号 */
                int16_t t = latest.temp10;
                if (t < 0) {
                    len = snprintf(out, sizeof(out),
                        "[AQI] TVOC=%u ug/m3  HCHO=%u ug/m3  CO2=%u ppm"
                        "  AQI=%u  T=-%d.%d C  RH=%d.%d%%\r\n",
                        latest.tvoc, latest.hcho, latest.co2, latest.aqi,
                        (int)((-t) / 10), (int)((-t) % 10),
                        (int)(latest.humi10 / 10), (int)(latest.humi10 % 10));
                } else {
                    len = snprintf(out, sizeof(out),
                        "[AQI] TVOC=%u ug/m3  HCHO=%u ug/m3  CO2=%u ppm"
                        "  AQI=%u  T=%d.%d C  RH=%d.%d%%\r\n",
                        latest.tvoc, latest.hcho, latest.co2, latest.aqi,
                        (int)(t / 10), (int)(t % 10),
                        (int)(latest.humi10 / 10), (int)(latest.humi10 % 10));
                }
            } else {
                len = snprintf(out, sizeof(out), "[AQI] No data (check SC-4M01A)\r\n");
            }

            HAL_UART_Transmit(&huart1, (uint8_t *)out, (uint16_t)len, 200);
        }
    }
}

/**
 * @brief USART4 任务
 *        BOT 板：BA111 EC/TDS 传感器采集（9600 bps），每 5 秒查询一次
 *        Top 板：通用接收（占位）
 */
void StartUart4Task(void *argument)
{
    uint8_t  buf[16];
    uint16_t n;
#if defined(MODBUS_BOARD_BOT)
    BA111_Data_t ec;
    char         out[64];
    int          len;
    TickType_t   last_wake = xTaskGetTickCount();
#endif

    for (;;) {
#if defined(MODBUS_BOARD_BOT)
        /* 发送 BA111 检测指令，等待 IDLE 中断触发通知 */
        BA111_Request(&huart4);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));

        n = UART_DMA_RX_Read(&huart4, buf, sizeof(buf));
        if (BA111_Parse(buf, n, &ec) == HAL_OK) {
            g_modbus_input_ec = ec.tds_ppm*2;
            len = snprintf(out, sizeof(out),
                           "[EC] TDS=%u ppm  T=%d.%d C\r\n",
                           ec.tds_ppm,
                           (int)(ec.temp10 / 10), (int)(ec.temp10 % 10));
            HAL_UART_Transmit(&huart1, (uint8_t *)out, (uint16_t)len, 200);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000));
#else
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        while ((n = UART_DMA_RX_Read(&huart4, buf, sizeof(buf))) > 0) {
            /* USER CODE BEGIN Uart4Task_Process */
            /* USER CODE END Uart4Task_Process */
        }
#endif
    }
}

/**
 * @brief 环境传感器采集任务
 *        [ENV]  = I2C1 AHT20(0x38) 一层温湿度 + BH1750(I2C2, 0x23) 光照
 *        [ENV1] = I2C2 AHT20(0x38) 箱外温度（仅 Top 板，BOT 板不接）
 *
 *        BH1750 ADDR 引脚接 GND 时地址 BH1750_ADDR_L (0x23)
 *        若 ADDR 接 VCC，改为 BH1750_ADDR_H (0x5C)
 */
void StartAht20Task(void *argument)
{
    char         buf[80];
    int          len;
    AHT20_Data_t aht20;       /* I2C1 AHT20：一层温湿度（两板均有） */
#if !defined(MODBUS_BOARD_BOT)
    AHT20_Data_t aht20_i2c2;  /* I2C2 AHT20：箱外温度（仅 Top 板） */
#endif
    uint32_t     lux;
    TickType_t   last_wake = xTaskGetTickCount();

    /* --- 初始化 I2C1 上的 AHT20（一层温湿度，两板均有）--- */
    while (AHT20_Init(&hi2c1) != HAL_OK) {
        len = snprintf(buf, sizeof(buf), "[ENV] AHT20 init failed, retrying...\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 200);
        osDelay(1000);
    }

    /* --- 初始化 BH1750（I2C2，内部等待 180ms 首次测量完成）--- */
    while (BH1750_Init(&hi2c2, BH1750_ADDR_L) != HAL_OK) {
        len = snprintf(buf, sizeof(buf), "[ENV] BH1750 init failed, retrying...\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 200);
        osDelay(1000);
    }

#if !defined(MODBUS_BOARD_BOT)
    /* --- 初始化 I2C2 上的 AHT20（仅 Top 板，用于箱外温度）--- */
    while (AHT20_Init(&hi2c2) != HAL_OK) {
        len = snprintf(buf, sizeof(buf), "[ENV1] AHT20 init failed, retrying...\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 200);
        osDelay(1000);
    }
    len = snprintf(buf, sizeof(buf), "[ENV] AHT20(I2C1) + BH1750 + AHT20(I2C2) ready\r\n");
#else
    len = snprintf(buf, sizeof(buf), "[ENV] AHT20(I2C1) + BH1750 ready\r\n");
#endif
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 200);

    for (;;) {
        /* 读 I2C1 AHT20（一层温湿度，内含 osDelay 80ms） */
        HAL_StatusTypeDef aht_ret = AHT20_Read(&hi2c1, &aht20);

        /* 读 BH1750（I2C2，连续模式直接取最新值） */
        HAL_StatusTypeDef bh_ret = BH1750_Read(&hi2c2, BH1750_ADDR_L, &lux);

#if !defined(MODBUS_BOARD_BOT)
        /* 读 I2C2 AHT20（箱外温度，内含 osDelay 80ms） */
        HAL_StatusTypeDef aht2_ret = AHT20_Read(&hi2c2, &aht20_i2c2);
#endif

        /* --- 更新 Modbus 输入寄存器 --- */
        if (aht_ret == HAL_OK) {
            g_modbus_input[MODBUS_REG_TEMP1]  = (int16_t)(aht20.temperature * 10.0f);
            g_modbus_input[MODBUS_REG_HUMID1] = (int16_t)(aht20.humidity    * 10.0f);
        }
        if (bh_ret == HAL_OK) {
            g_modbus_input[MODBUS_REG_LIGHT1] = (int16_t)lux;
        }
#if !defined(MODBUS_BOARD_BOT)
        if (aht2_ret == HAL_OK) {
            g_modbus_input[MODBUS_REG_TEMP2]  = (int16_t)(aht20_i2c2.temperature * 10.0f);
        }
#endif

        /* --- 输出 I2C1：AHT20 + BH1750 --- */
        if (aht_ret == HAL_OK && bh_ret == HAL_OK) {
            int32_t t10 = (int32_t)(aht20.temperature * 10.0f);
            int32_t h10 = (int32_t)(aht20.humidity    * 10.0f);

            if (t10 < 0) {
                len = snprintf(buf, sizeof(buf),
                               "[ENV INSIDE] T=-%d.%d C  RH=%d.%d%%  Lux=%lu\r\n",
                               (int)((-t10) / 10), (int)((-t10) % 10),
                               (int)(h10    / 10), (int)(h10    % 10),
                               (unsigned long)lux);
            } else {
                len = snprintf(buf, sizeof(buf),
                               "[ENV INSIDE] T=%d.%d C  RH=%d.%d%%  Lux=%lu\r\n",
                               (int)(t10 / 10), (int)(t10 % 10),
                               (int)(h10 / 10), (int)(h10 % 10),
                               (unsigned long)lux);
            }
        } else {
            len = snprintf(buf, sizeof(buf),
                           "[ENV INSIDE] Read error (AHT20:%d BH1750:%d)\r\n",
                           (int)aht_ret, (int)bh_ret);
        }
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 200);

#if !defined(MODBUS_BOARD_BOT)
        /* --- 输出 I2C2：AHT20（箱外温度）--- */
        if (aht2_ret == HAL_OK) {
            int32_t t10 = (int32_t)(aht20_i2c2.temperature * 10.0f);
            int32_t h10 = (int32_t)(aht20_i2c2.humidity    * 10.0f);

            if (t10 < 0) {
                len = snprintf(buf, sizeof(buf),
                               "[ENV OUTSIDE] T=-%d.%d C  RH=%d.%d%%\r\n",
                               (int)((-t10) / 10), (int)((-t10) % 10),
                               (int)(h10    / 10), (int)(h10    % 10));
            } else {
                len = snprintf(buf, sizeof(buf),
                               "[ENV OUTSIDE] T=%d.%d C  RH=%d.%d%%\r\n",
                               (int)(t10 / 10), (int)(t10 % 10),
                               (int)(h10 / 10), (int)(h10 % 10));
            }
        } else {
            len = snprintf(buf, sizeof(buf),
                           "[ENV OUTSIDE] Read error (AHT20:%d)\r\n",
                           (int)aht2_ret);
        }
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 200);
#endif

        /* 精确 5 秒间隔（补偿传感器读取�?�时�? */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000));
    }
}

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

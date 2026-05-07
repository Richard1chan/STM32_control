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
#include "nfc_reader.h"
#include "modbus_slave.h"
#include "debug_uart.h"
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
static osThreadId_t nfcTaskHandle;
static const osThreadAttr_t nfcTask_attributes = {
  .name       = "nfcTask",
  .priority   = (osPriority_t)osPriorityNormal,
  .stack_size = 512
};

static osThreadId_t modbusTaskHandle;
static const osThreadAttr_t modbusTask_attributes = {
  .name       = "modbusTask",
  .priority   = (osPriority_t)osPriorityAboveNormal,
  .stack_size = 512
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void NfcPollTask(void *argument);
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
  NFC_Init();
  Modbus_Init();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  g_nfc_mutex = osMutexNew(NULL);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  g_modbus_rx_sem = osSemaphoreNew(1, 0, NULL);
  g_modbus_tx_sem = osSemaphoreNew(1, 0, NULL);
  g_nfc_tx_sem    = osSemaphoreNew(1, 0, NULL);
  g_nfc_rx_sem    = osSemaphoreNew(1, 0, NULL);
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
  nfcTaskHandle    = osThreadNew(NfcPollTask,   NULL, &nfcTask_attributes);
  modbusTaskHandle = osThreadNew(Modbus_Task,   NULL, &modbusTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* Debug_Init must be after semaphore/mutex creation */
  Debug_Init();
  Debug_Printf("\r\n=== slave_control boot, addr=0x%02X ===\r\n", MODBUS_SLAVE_ADDR);
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
    osDelay(2000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void NfcPollTask(void *argument)
{
    (void)argument;
    Debug_Printf("[NFC] Poll task started\r\n");
    for (;;) {
        NFC_PollAll();
        osDelay(1000);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART6) {
        g_modbus_rx_len = Size;
        osSemaphoreRelease(g_modbus_rx_sem);
    } else if (huart->Instance == USART1 || huart->Instance == USART3 ||
               huart->Instance == USART4 || huart->Instance == USART5) {
        g_nfc_rx_size = Size;
        osSemaphoreRelease(g_nfc_rx_sem);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6) {
        osSemaphoreRelease(g_modbus_tx_sem);
    } else if (huart->Instance == USART1 || huart->Instance == USART3 ||
               huart->Instance == USART4 || huart->Instance == USART5) {
        osSemaphoreRelease(g_nfc_tx_sem);
    }
}
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

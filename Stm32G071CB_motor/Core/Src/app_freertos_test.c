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
/* USER CODE BEGIN Variables */

uint8_t rx_buf1;
uint8_t rx_buf4;

osMessageQueueId_t uartMsgQueue;   // ?????????? (??????)
osMessageQueueId_t uart1RxQueue;   // ??:USART1 ??????
osMessageQueueId_t uart4RxQueue;   // ??:USART4 ??????


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
  

  uartMsgQueue = osMessageQueueNew(10, sizeof(char*), NULL);
  uart1RxQueue = osMessageQueueNew(32, sizeof(uint8_t), NULL); 
  uart4RxQueue = osMessageQueueNew(32, sizeof(uint8_t), NULL);

  HAL_UART_Receive_DMA(&huart1, &rx_buf1, 1);
  HAL_UART_Receive_DMA(&huart4, &rx_buf4, 1);

  uint32_t last_toggle_ticks = osKernelGetTickCount();
  const char* periodic_msg = "5 Seconds Tick!\r\n";

  /* Infinite loop */
  for(;;)
  {
    char* msg;
    uint8_t rx_byte;
   

    osStatus_t status = osMessageQueueGet(uartMsgQueue, &msg, NULL, 10);
    if (status == osOK) 
    {
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
      HAL_UART_Transmit(&huart4, (uint8_t*)msg, strlen(msg), 100);
    }


    while (osMessageQueueGet(uart1RxQueue, &rx_byte, NULL, 0) == osOK) 
    {
      HAL_UART_Transmit(&huart1, &rx_byte, 1, 10);
    }

    while (osMessageQueueGet(uart4RxQueue, &rx_byte, NULL, 0) == osOK) 
    {
      HAL_UART_Transmit(&huart4, &rx_byte, 1, 10);
			
    }
 

    uint32_t current_ticks = osKernelGetTickCount();
    if (current_ticks - last_toggle_ticks >= 5000) 
    {
      last_toggle_ticks = current_ticks;

      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7);
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10|GPIO_PIN_11);

//      HAL_UART_Transmit(&huart1, (uint8_t*)periodic_msg, strlen(periodic_msg), 100);
//      HAL_UART_Transmit(&huart4, (uint8_t*)periodic_msg, strlen(periodic_msg), 100);
    }
		osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
  * @brief  DMA 
  */
/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
  * @brief  DMA 
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    if (uart1RxQueue != NULL) {
      osMessageQueuePut(uart1RxQueue, &rx_buf1, 0, 0); 
    }
  } 
  else if (huart->Instance == USART4) {
    if (uart4RxQueue != NULL) {
      osMessageQueuePut(uart4RxQueue, &rx_buf4, 0, 0);
    }
  }
}

/**
  * @brief  GPIO 
  */
/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */


static uint32_t last_exti_time[16] = {0}; 
#define DEBOUNCE_DELAY 50 


uint8_t Is_Valid_Edge(uint16_t GPIO_Pin)
{
  uint32_t current_time = HAL_GetTick(); 
  uint8_t pin_idx = 0;
  

  for (int i = 0; i < 16; i++) {
    if (GPIO_Pin == (1 << i)) {
      pin_idx = i;
      break;
    }
  }

 
  if ((current_time - last_exti_time[pin_idx]) < DEBOUNCE_DELAY) {
    return 0; 
  }
  

  last_exti_time[pin_idx] = current_time;
  return 1; 
}


void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_12 || GPIO_Pin == GPIO_PIN_13 || 
      GPIO_Pin == GPIO_PIN_14 || GPIO_Pin == GPIO_PIN_15 || 
      GPIO_Pin == GPIO_PIN_6) 
  {
   
    if (Is_Valid_Edge(GPIO_Pin)) 
    {
      const char* msg = "open\r\n";
      if (uartMsgQueue != NULL) {
        osMessageQueuePut(uartMsgQueue, &msg, 0, 0); 
      }
    }
  }
}


void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_12 || GPIO_Pin == GPIO_PIN_13 || 
      GPIO_Pin == GPIO_PIN_14 || GPIO_Pin == GPIO_PIN_15 || 
      GPIO_Pin == GPIO_PIN_6) 
  {
  
    if (Is_Valid_Edge(GPIO_Pin)) 
    {
      const char* msg = "close\r\n";
      if (uartMsgQueue != NULL) {
        osMessageQueuePut(uartMsgQueue, &msg, 0, 0); 
      }
    }
  }
}

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

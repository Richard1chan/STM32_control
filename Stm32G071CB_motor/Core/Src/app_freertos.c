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

#define SLAVE_A_ID 0x19
#define SLAVE_B_ID 0x1A

#define SLAVE_ID  SLAVE_B_ID

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

uint8_t rx_buf4;

osMessageQueueId_t modbusRxQueue;  // Modbus消息队列
osThreadId_t modbusTaskHandle;     
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
/* USER CODE BEGIN FunctionPrototypes */
uint16_t Modbus_CRC16(uint8_t *puchMsg, uint16_t usDataLen);
uint16_t ReadModbusStatus(void);
void WriteModbusStatus(uint16_t val);
void ProcessModbusFrame(uint8_t *frame, uint16_t len);
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
  
  uint8_t rx_frame[128];
  uint16_t rx_len = 0;
  //创建消息队列
  modbusRxQueue = osMessageQueueNew(128, sizeof(uint8_t), NULL); // Modbus 队列
  //�?启DMA接收
  HAL_UART_Receive_DMA(&huart4, &rx_buf4, 1);

  /* Infinite loop */
  for(;;)
  {
		    uint8_t byte;
    // 等待接收超时设定�? 5ms (波特�?115200?,判定帧间�?)
    osStatus_t status = osMessageQueueGet(modbusRxQueue, &byte, NULL, 5);

    if (status == osOK) {
      if (rx_len < sizeof(rx_frame)) {
        rx_frame[rx_len++] = byte;
      }
    } else {
      // 超时，一帧数据接收完�?
      if (rx_len >= 8) { // Modbus标准帧最小长�?8字节
			
        ProcessModbusFrame(rx_frame, rx_len);
      }
      rx_len = 0; // 清缓�?
    }
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

// ------------------- DMA -------------------
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART4) {
    // USART4 收到数据，塞入Modbus队列
    if (modbusRxQueue != NULL) {
      osMessageQueuePut(modbusRxQueue, &rx_buf4, 0, 0);
    }
  } 

}

uint16_t ReadModbusStatus(void) {
  uint16_t status = 0;
  //输出部分
  if (HAL_GPIO_ReadPin(WATER_PUMP1_PORT, WATER_PUMP1_PIN) == GPIO_PIN_SET) status |= (1 << 0);
  if (HAL_GPIO_ReadPin(WATER_PUMP2_PORT, WATER_PUMP2_PIN) == GPIO_PIN_SET) status |= (1 << 1);
  if (HAL_GPIO_ReadPin(MICRO_PUMP0_PORT, MICRO_PUMP0_PIN) == GPIO_PIN_SET) status |= (1 << 2);
  if (HAL_GPIO_ReadPin(MICRO_PUMP1_PORT, MICRO_PUMP1_PIN) == GPIO_PIN_SET) status |= (1 << 3);
  if (HAL_GPIO_ReadPin(MICRO_PUMP2_PORT, MICRO_PUMP2_PIN) == GPIO_PIN_SET) status |= (1 << 4);
  if (HAL_GPIO_ReadPin(MICRO_PUMP3_PORT, MICRO_PUMP3_PIN) == GPIO_PIN_SET) status |= (1 << 5);

  if (HAL_GPIO_ReadPin(SOLENOID0_PORT, SOLENOID0_PIN) == GPIO_PIN_SET) status |= (1 << 6);
  if (HAL_GPIO_ReadPin(SOLENOID1_PORT, SOLENOID1_PIN) == GPIO_PIN_SET) status |= (1 << 7);
  if (HAL_GPIO_ReadPin(SOLENOID2_PORT, SOLENOID2_PIN) == GPIO_PIN_SET) status |= (1 << 8);
  if (HAL_GPIO_ReadPin(SOLENOID3_PORT, SOLENOID3_PIN)== GPIO_PIN_SET) status |= (1 << 9);
  if (HAL_GPIO_ReadPin(FAN0_FAN1_PORT, FAN0_FAN1_PIN)== GPIO_PIN_SET) status |= (1 << 10); 

 //输入部分
  if (HAL_GPIO_ReadPin(DOOR_PORT, DOOR_PIN) == GPIO_PIN_SET) status |= (1 << 11); //
  if (HAL_GPIO_ReadPin(PALLET0_PORT, PALLET0_PIN)== GPIO_PIN_SET) status |= (1 << 12); // 
  if (HAL_GPIO_ReadPin(PALLET1_PORT, PALLET1_PIN)== GPIO_PIN_SET) status |= (1 << 13); // 
  if (HAL_GPIO_ReadPin(PALLET2_PORT, PALLET2_PIN)== GPIO_PIN_SET) status |= (1 << 14); // 
  if (HAL_GPIO_ReadPin(PALLET3_PORT, PALLET3_PIN)== GPIO_PIN_SET) status |= (1 << 15); // 

  return status;
}

void WriteModbusStatus(uint16_t val) {
  HAL_GPIO_WritePin(WATER_PUMP1_PORT, WATER_PUMP1_PIN, (val & (1<<0)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(WATER_PUMP2_PORT, WATER_PUMP2_PIN, (val & (1<<1)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP0_PORT, MICRO_PUMP0_PIN, (val & (1<<2)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP1_PORT, MICRO_PUMP1_PIN, (val & (1<<3)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP2_PORT, MICRO_PUMP2_PIN, (val & (1<<4)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MICRO_PUMP3_PORT, MICRO_PUMP3_PIN, (val & (1<<5)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  HAL_GPIO_WritePin(SOLENOID0_PORT, SOLENOID0_PIN, (val & (1<<6)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID1_PORT, SOLENOID1_PIN, (val & (1<<7)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID2_PORT, SOLENOID2_PIN, (val & (1<<8)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SOLENOID3_PORT, SOLENOID3_PIN, (val & (1<<9)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(FAN0_FAN1_PORT, FAN0_FAN1_PIN, (val & (1<<10))? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// ------------------- Modbus 帧解�?-------------------
void ProcessModbusFrame(uint8_t *frame, uint16_t len) {
  // 1. �?查从机地�?
  if (frame[0] != SLAVE_ID) return; 

  // 2. �?查CRC校验
  uint16_t calc_crc = Modbus_CRC16(frame, len - 2);
  uint16_t recv_crc = frame[len-2] | (frame[len-1] << 8);
  if (calc_crc != recv_crc) return;
  // 3. 解析功能�?
// 3. ?????
  if (frame[1] == 0x06) // ????1?:0x06 ??????
  {
    if (len < 8) return; // ????2?:0x06 ???????? 8 ??

    uint16_t reg_addr = (frame[2] << 8) | frame[3];
    // ????3?:0x06 ???????????,? frame[4] ? frame[5]
    uint16_t data = (frame[4] << 8) | frame[5]; 

    // ??????? 0x0000
    if (reg_addr == 0x0000) {
      WriteModbusStatus(data); // ??????

      // ????4?:??????0x06 ???????????6???????
      uint8_t resp[8];
      resp[0] = SLAVE_ID;         // ????
      resp[1] = 0x06;         // ???
      resp[2] = frame[2];     // ???????
      resp[3] = frame[3];     // ???????
      resp[4] = frame[4];     // ???????
      resp[5] = frame[5];     // ???????
      
      uint16_t resp_crc = Modbus_CRC16(resp, 6);
      resp[6] = resp_crc & 0xFF;
      resp[7] = (resp_crc >> 8) & 0xFF;
      
      // ??:??? HAL_UART_Transmit ??? ModbusTask ???,
      // ????????????,??????!
      HAL_UART_Transmit(&huart4, resp, 8, 100);
    }
  }
  else if (frame[1] == 0x04) // 0x04 读输入寄存器
  {
    uint16_t reg_addr = (frame[2] << 8) | frame[3];
    uint16_t reg_qty = (frame[4] << 8) | frame[5];

    // 判断地址是否�?0x0000，且仅读�?个寄存器
    if (reg_addr == 0x0000 && reg_qty == 1) {
      uint16_t data = ReadModbusStatus(); // 获取硬件状�??

      // 构建应答�?
      uint8_t resp[7];
      resp[0] = SLAVE_ID;
      resp[1] = 0x04;
      resp[2] = 0x02; // 字节�?
      resp[3] = (data >> 8) & 0xFF; // 数据高位
      resp[4] = data & 0xFF;        // 数据地位
      uint16_t resp_crc = Modbus_CRC16(resp, 5);
      resp[5] = resp_crc & 0xFF;
      resp[6] = (resp_crc >> 8) & 0xFF;
      HAL_UART_Transmit(&huart4, resp, 7, 100);
    }
  }
}

// ------------------- CRC16 算法 -------------------
uint16_t Modbus_CRC16(uint8_t *puchMsg, uint16_t usDataLen) {
  uint16_t wCRCin = 0xFFFF;
  uint16_t wCPoly = 0xA001;
  while (usDataLen--) {
    wCRCin ^= *puchMsg++;
    for (int i = 0; i < 8; i++) {
      if (wCRCin & 0x01) {
        wCRCin >>= 1;
        wCRCin ^= wCPoly;
      } else {
        wCRCin >>= 1;
      }
    }
  }
  return wCRCin;
}

// // ------------------- GPIO 去抖动与中断逻辑判断 -------------------
// static uint32_t last_exti_time[16] = {0}; 
// #define DEBOUNCE_DELAY 50 

// uint8_t Is_Valid_Edge(uint16_t GPIO_Pin) {
//   uint32_t current_time = HAL_GetTick(); 
//   uint8_t pin_idx = 0;
//   for (int i = 0; i < 16; i++) {
//     if (GPIO_Pin == (1 << i)) {
//       pin_idx = i;
//       break;
//     }
//   }
//   if ((current_time - last_exti_time[pin_idx]) < DEBOUNCE_DELAY) { return 0; }
//   last_exti_time[pin_idx] = current_time;
//   return 1; 
// }

// void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
//   if (GPIO_Pin == GPIO_PIN_12 || GPIO_Pin == GPIO_PIN_13 || 
//       GPIO_Pin == GPIO_PIN_14 || GPIO_Pin == GPIO_PIN_15 || GPIO_Pin == GPIO_PIN_6) 
//   {
//     if (Is_Valid_Edge(GPIO_Pin)) {
//       const char* msg = "open\r\n";
//       if (uartMsgQueue != NULL) { osMessageQueuePut(uartMsgQueue, &msg, 0, 0); }
//     }
//   }
// }

// void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
//   if (GPIO_Pin == GPIO_PIN_12 || GPIO_Pin == GPIO_PIN_13 || 
//       GPIO_Pin == GPIO_PIN_14 || GPIO_Pin == GPIO_PIN_15 || GPIO_Pin == GPIO_PIN_6) 
//   {
//     if (Is_Valid_Edge(GPIO_Pin)) {
//       const char* msg = "close\r\n";
//       if (uartMsgQueue != NULL) { osMessageQueuePut(uartMsgQueue, &msg, 0, 0); }
//     }
//   }
// }
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

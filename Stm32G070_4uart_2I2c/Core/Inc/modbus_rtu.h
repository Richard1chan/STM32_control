/**
 * @file    modbus_rtu.h
 * @brief   Modbus RTU Slave driver for 温控板 (USART2 / RS485)
 *
 * Board selection — define ONE macro in project settings (or here):
 *   (nothing)          → MODBUS_BOARD_TOP, Slave 0x01
 *   MODBUS_BOARD_BOT   → Slave 0x02
 *
 * Supported function codes (both boards):
 *   FC=0x04  Read Input Registers
 *   FC=0x06  Write Single Holding Register
 *   FC=0x10  Write Multiple Holding Registers
 *
 * ── 温控板-Top (Slave 0x01) ──────────────────────────────────────────────
 * Input registers (FC=0x04):
 *   0x0000  MODBUS_REG_TEMP1   INT16  x10  一层温度 (°C×10)
 *   0x0001  MODBUS_REG_TEMP2   INT16  x10  箱外温度 (°C×10)
 *   0x0002  MODBUS_REG_HUMID1  UINT16 x10  一层湿度 (%RH×10)
 *   0x0003  MODBUS_REG_CO2_1   UINT16 x1   一层CO₂  (ppm)
 *   0x0004  MODBUS_REG_LIGHT1  UINT16 x1   一层光照 (lux)
 *   0x0005  [PWM回显] 制冷片1 PWM  → g_modbus_hold[0]
 *   0x0006  [PWM回显] 制冷片2 PWM  → g_modbus_hold[1]
 *   0x0007  [PWM回显] 风扇1 PWM    → g_modbus_hold[2]
 *   0x0008  [PWM回显] 风扇2 PWM    → g_modbus_hold[3]
 *
 * ── 温控板-BOT (Slave 0x02) ──────────────────────────────────────────────
 * Input registers (FC=0x04):
 *   0x0000  MODBUS_REG_TEMP1   INT16  x10  二层温度 (°C×10)
 *   0x0001  MODBUS_REG_HUMID1  UINT16 x10  二层湿度 (%RH×10)
 *   0x0002  MODBUS_REG_CO2_1   UINT16 x1   二层CO₂  (ppm)
 *   0x0003  MODBUS_REG_LIGHT1  UINT16 x1   二层光照 (lux)
 *   0x0004  [PWM回显] 制冷片1 PWM  → g_modbus_hold[0]
 *   0x0005  [PWM回显] 制冷片2 PWM  → g_modbus_hold[1]
 *   0x0006  [PWM回显] 风扇1 PWM    → g_modbus_hold[2]
 *   0x0007  [PWM回显] 风扇2 PWM    → g_modbus_hold[3]
 *
 * Holding registers (FC=0x06 / FC=0x10, both boards):
 *   0x0000  MODBUS_REG_PWM_COOL1  UINT16  制冷片1 PWM
 *   0x0001  MODBUS_REG_PWM_COOL2  UINT16  制冷片2 PWM
 *   0x0002  MODBUS_REG_PWM_FAN1   UINT16  风扇1 PWM → TIM1_CH1 (PA8)
 *   0x0003  MODBUS_REG_PWM_FAN2   UINT16  风扇2 PWM → TIM1_CH2 (PB3)
 */

#ifndef __MODBUS_RTU_H__
#define __MODBUS_RTU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ── 板型选择（在项目预处理宏中定义 MODBUS_BOARD_BOT 切换至 BOT 板）── */
#if defined(MODBUS_BOARD_BOT)

  #define MODBUS_SLAVE_ADDR         0x02u
  /* 传感器寄存器数（0x0000~0x0003，g_modbus_input 大小） */
  #define MODBUS_INPUT_SENSOR_CNT   4u
  /* FC=0x04 可读总数（含 PWM 回显 0x0004~0x0007 + EC 0x0008） */
  #define MODBUS_INPUT_REG_COUNT    9u
  /* EC 传感器寄存器地址（固定 0x0008，位于 PWM 回显之后，有单独变量存储） */
  #define MODBUS_REG_EC_ADDR        8u

  #define MODBUS_REG_TEMP1          0u   /* 二层温度,  INT16  x10 */
  #define MODBUS_REG_HUMID1         1u   /* 二层湿度,  UINT16 x10 */
  #define MODBUS_REG_CO2_1          2u   /* 二层CO₂,  UINT16 x1  */
  #define MODBUS_REG_LIGHT1         3u   /* 二层光照,  UINT16 x1  */

#else  /* MODBUS_BOARD_TOP (default) */

  #define MODBUS_SLAVE_ADDR         0x01u
  /* 传感器寄存器数（0x0000~0x0004，g_modbus_input 大小） */
  #define MODBUS_INPUT_SENSOR_CNT   5u
  /* FC=0x04 可读总数（含 PWM 回显 0x0005~0x0008） */
  #define MODBUS_INPUT_REG_COUNT    9u

  #define MODBUS_REG_TEMP1          0u   /* 一层温度,  INT16  x10 */
  #define MODBUS_REG_TEMP2          1u   /* 箱外温度,  INT16  x10 */
  #define MODBUS_REG_HUMID1         2u   /* 一层湿度,  UINT16 x10 */
  #define MODBUS_REG_CO2_1          3u   /* 一层CO₂,  UINT16 x1  */
  #define MODBUS_REG_LIGHT1         4u   /* 一层光照,  UINT16 x1  */

#endif /* MODBUS_BOARD_BOT */

/* ── 保持寄存器（两板相同）──────────────────────────────────────────── */
#define MODBUS_HOLD_REG_COUNT       4u
#define MODBUS_REG_PWM_COOL1        0u   /* 制冷片1 PWM */
#define MODBUS_REG_PWM_COOL2        1u   /* 制冷片2 PWM */
#define MODBUS_REG_PWM_FAN1         2u   /* 风扇1 PWM → TIM1_CH1 */
#define MODBUS_REG_PWM_FAN2         3u   /* 风扇2 PWM → TIM1_CH2 */

/* ── 共享寄存器（跨任务读写）───────────────────────────────────────── */
/* 传感器任务写，Modbus 任务读；大小为 MODBUS_INPUT_SENSOR_CNT */
extern volatile int16_t  g_modbus_input[MODBUS_INPUT_SENSOR_CNT];
/* Modbus 任务写，执行器任务读 */
extern volatile uint16_t g_modbus_hold[MODBUS_HOLD_REG_COUNT];
#if defined(MODBUS_BOARD_BOT)
/* EC 传感器（UART4 BA111）写，Modbus 任务读；对应 FC=0x04 地址 MODBUS_REG_EC_ADDR */
extern volatile uint16_t g_modbus_input_ec;
#endif

void Modbus_Init(void);
void Modbus_Process(const uint8_t *frame, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_RTU_H__ */

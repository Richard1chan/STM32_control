# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is firmware for a **绿芽种植箱 (Green Sprout Planting Box)** control board based on the **STM32G071CBUx** (Cortex-M0+, UFQFPN48). The board acts as a **Modbus RTU slave** that controls irrigation and environmental actuators via GPIO outputs and reads sensor inputs.

## Build System

The project is built with **Keil MDK-ARM V5** (Windows IDE). There is no Makefile or CLI build system. Open `MDK-ARM/Stm32G071CB_motor.uvprojx` in Keil uVision to build and flash.

- Build output: `MDK-ARM/Stm32G071CB_motor/Stm32G071CB_motor.hex`
- Target device: STM32G071CBUx, pack: `Keil.STM32G0xx_DFP.1.2.0`
- Clock: 16 MHz HSI internal oscillator (no PLL active)

## Architecture

### RTOS

FreeRTOS (CMSIS-OS v2) with a single task `defaultTask` (256×4 bytes stack, normal priority). TIM6 is used as the HAL timebase instead of SysTick, because SysTick is taken by FreeRTOS.

### Modbus RTU Slave (active firmware: `Core/Src/app_freertos.c`)

USART4 (PA0 TX / PA1 RX, 115200 8N1) is the Modbus interface. DMA runs in circular mode transferring one byte at a time into `rx_buf4`. Each received byte is pushed from `HAL_UART_RxCpltCallback` into a FreeRTOS message queue (`modbusRxQueue`, 32 items).

**帧结束检测**：UART4 IDLE 中断（`USART3_4_LPUART1_IRQHandler` → `Modbus_IdleCallback`）在 RX 线静默一个字符时间后释放二值信号量 `modbusIdleSem`。任务阻塞在此信号量上（3 ms 超时兜底），唤醒后排空队列再解析帧。响应延迟从原来的 5 ms 缩短到 <1 ms。

**Slave address**: `SLAVE_B_ID = 0x1A` (change to `SLAVE_A_ID = 0x19` at the `#define SLAVE_ID` line for board A).

Supported function codes:
- **FC 0x06** – Write Single Register at address `0x0000`: sets all actuator outputs from a 16-bit bitmask, echoes the frame back.
- **FC 0x04** – Read Input Registers at address `0x0000`, quantity 1: returns the full 16-bit I/O status.

### Output/Input Bit Mapping (register 0x0000)

| Bit | Signal | GPIO |
|-----|--------|------|
| 0 | WATER_PUMP1 | PA7 |
| 1 | WATER_PUMP2 | PB0 |
| 2 | MICRO_PUMP0 | PB1 |
| 3 | MICRO_PUMP1 | PB2 |
| 4 | MICRO_PUMP2 | PB10 |
| 5 | MICRO_PUMP3 | PB11 |
| 6 | SOLENOID0 | PA3 |
| 7 | SOLENOID1 | PA2 |
| 8 | SOLENOID2 | PA5 |
| 9 | SOLENOID3 | PA6 |
| 10 | FAN0/FAN1 | PA4 |
| 11 | DOOR sensor (input) | PB12 |
| 12 | PALLET0 sensor (input) | PB15 |
| 13 | PALLET1 sensor (input) | PC6 |
| 14 | PALLET2 sensor (input) | PB14 |
| 15 | PALLET3 sensor (input) | PB13 |

Bits 0–10 are read/write (actuators). Bits 11–15 are read-only GPIO inputs (door magnetic sensor + 4 pallet position sensors). FC 0x06 calls `WriteModbusStatus()` which only drives bits 0–10; bits 11–15 are ignored on write.

**UART4 总线错误恢复**：当总线上其他设备重启时，RS485 线上的电气毛刺会触发 USART4 的帧错误（FE）或过载错误（ORE）。STM32 HAL 检测到错误后会调用 `HAL_DMA_Abort_IT` 停止 DMA 传输，若不处理则 UART4 永久失去接收能力。修复方案：实现 `HAL_UART_ErrorCallback`，在 ISR 中仅置 `uart4_need_restart` 标志并释放 `modbusIdleSem`；任务循环每次醒来优先检查该标志，若置位则调用 `osMessageQueueReset` 清空脏数据，再重新调用 `HAL_UART_Receive_DMA` + `__HAL_UART_ENABLE_IT(IDLE)` 恢复接收。错误累计次数记录在 `stat_uart4_errs`，恢复时通过 debug 串口打印。

### USART1 — Debug 串口

USART1 (PA9 TX / PA10 RX, 115200 8N1) 作为只写调试口。`debug_printf()` 通过阻塞式 `HAL_UART_Transmit` 向 UART1 输出文本，只能在任务上下文（非 ISR）调用。调试输出包括启动信息、每次 Modbus 收发结果、CRC 错误、非法功能码及发送失败计数。

### Archived / Test Code

- `Core/Src/app_freertos_test.c` — earlier test that echoed both UARTs and toggled all outputs every 5 s. Not compiled in the active build.
- `Core/Src/app_freertos.bak` — backup snapshot of a previous version of `app_freertos.c`.

## STM32CubeMX Code Generation Rules

The `.ioc` file is the authoritative hardware configuration. CubeMX regenerates all peripheral init code. **Only edit code inside `/* USER CODE BEGIN … */ … /* USER CODE END … */` blocks**; anything outside those markers will be overwritten on the next CubeMX regeneration.

All application logic lives in the USER CODE sections of `Core/Src/app_freertos.c`. The CubeMX-generated files (`gpio.c`, `usart.c`, `dma.c`, `main.c`) should only be modified by re-running CubeMX.

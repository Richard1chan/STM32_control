# slave_control 项目说明

## 硬件平台

| 项目 | 值 |
|------|-----|
| MCU | STM32G0B1RCTx |
| 封装 | LQFP64 |
| 系统时钟 | 16 MHz（HSI） |
| AHB/APB 频率 | 16 MHz |
| PLL 最大输出 | 64 MHz（已配置，未作为主时钟） |

## 开发工具

- **IDE / 编译器**：Keil MDK-ARM V5（`.uvprojx` 工程）
- **HAL 固件包**：STM32Cube FW_G0 V1.4.1
- **CubeMX 版本**：6.2.1
- **编译优化**：O3（Level 6）

## RTOS

FreeRTOS，CMSIS-RTOS V2 接口。

| 参数 | 值 |
|------|-----|
| HAL 时基 | TIM6 |
| Heap 大小 | 0x200（512 B），heap_4 算法 |
| Stack 大小 | 0x400（1024 B） |
| 默认任务 | `defaultTask`，优先级 24，栈 128 words，入口 `StartDefaultTask` |

## 外设配置

### ADC1

| 通道 | 引脚 |
|------|------|
| IN6  | PA6  |
| IN8  | PB0  |

常规转换，单次模式。

### UART / USART（全部异步模式，收发均使用 DMA）

| 外设 | TX 引脚 | RX 引脚 | DMA TX | DMA RX |
|------|---------|---------|--------|--------|
| USART1 | PC4 | PC5 | DMA1_Ch2 | DMA1_Ch1 |
| USART3 | PB2 | PD9 | DMA1_Ch6 | DMA1_Ch5 |
| USART4 | PA0 | PC11 | DMA2_Ch1 | DMA1_Ch7 |
| USART5 | PC12 | PD2 | DMA2_Ch2 | DMA2_Ch3 |
| USART6 | PC0 | PC1 | DMA2_Ch5 | DMA2_Ch4 |
| LPUART1 | PB11 | PB10 | DMA1_Ch4 | DMA1_Ch3 |

所有 DMA RX 均通过 `HAL_UARTEx_ReceiveToIdle_DMA` 启动，并在启动后立即禁用 HT 中断（`__HAL_DMA_DISABLE_IT(..., DMA_IT_HT)`），防止半满点误触发回调。HT = Half Transfer（半传输）中断，是 DMA 控制器的一个中断事件。

**实际运行波特率**（用户代码在 `USER CODE Init 2` 段覆盖 CubeMX 默认值）：

| 外设 | 实际波特率 | 用途 |
|------|-----------|------|
| USART1 | **9600** | NFC522 READER1 |
| USART3 | **9600** | NFC522 READER2 |
| USART4 | **9600** | NFC522 READER3 |
| USART5 | **9600** | NFC522 READER4 |
| USART6 | 115200 | **RS485 Modbus RTU 从机**（硬件自动方向控制） |
| LPUART1 | 115200 | **Debug 串口**（TX-only，PB11 接 USB-TTL，阻塞发送，无 DMA） |

### I2C1

- SCL：PA9
- SDA：PA10

### 定时器（PWM 输出 / 输入捕获）

| 定时器 | CH1 | CH2 | CH3 | CH4 |
|--------|-----|-----|-----|-----|
| TIM1 | PA8（输入捕获，TI1FP1 触发） | PC9（PWM） | PC10（PWM） | PA11（PWM） |
| TIM2 | PA5（输入捕获，TI1FP1 触发） | PA1（PWM） | PA2（PWM） | PA3（PWM） |
| TIM3 | PC6（输入捕获，TI1FP1 触发） | PA7（PWM） | PC8（PWM） | PB1（Output Compare） |
| TIM4 | PB6（PWM） | PB7（PWM） | PB8（PWM） | PB9（PWM） |
| TIM15 | PB14（PWM） | PC2（PWM） |  |  |

TIM1/TIM2/TIM3 的 CH1 均配置为输入捕获（TriggerSource_TI1FP1），其余通道为 PWM 输出。

### GPIO

**输出引脚**（已锁定）：

| 引脚 | 用途 |
|------|------|
| PA12 [PA10] | GPIO_Output |
| PB5 | GPIO_Output |
| PD3 | GPIO_Output |
| PD8 | GPIO_Output |

**外部中断（EXTI）**：

| 引脚 | 中断线 |
|------|--------|
| PD5 | EXTI5 |
| PD6 | EXTI6 |

### NVIC 优先级

| 中断 | 优先级 |
|------|--------|
| DMA 各通道 | 3 |
| EXTI4_15 | 3（允许抢占） |
| HardFault / NMI | 0 |
| FreeRTOS SVC / PendSV / SysTick | 0 / 3 / 3 |

## 应用功能：NFC 读卡 + Modbus RTU 从机

本板实现系统中的 NFC 读卡从站，符合 `RS485-Modbus RTU通信协议.md` 第 3 节定义。

### 角色与地址

| 参数 | 值 |
|------|-----|
| 系统角色 | RS485 Modbus RTU **从机** |
| 从站地址 | `0x0A`（READER1-4）或 `0x0B`（READER5-8），通过 `modbus_slave.h` 中 `MODBUS_SLAVE_ADDR` 宏配置 |
| 支持功能码 | FC=0x04（读输入寄存器） |

### NFC 读卡器映射

| 读卡器 | UART | TX | RX | Modbus 寄存器基址 |
|--------|------|----|----|---------------|
| READER1 | USART1 | PC4 | PC5 | 0x0000 |
| READER2 | USART3 | PB2 | PD9 | 0x0004 |
| READER3 | USART4 | PA0 | PC11 | 0x0008 |
| READER4 | USART5 | PC12 | PD2 | 0x000C |

每个读卡器占 4 个寄存器，存储格式：

| 偏移 | 内容 |
|------|------|
| +0 | 卡 ID 字节 0-1（大端） |
| +1 | 卡 ID 字节 2-3（大端） |
| +2 | 卡片类型（2字节） |
| +3 | 0x0000（预留，协议兼容 7 字节 UID 位） |

无卡或通信超时时寄存器全部清零。

### NFC522-V3.1 通信协议

- 波特率：9600 bps，8-N-1，LSB first
- 读卡命令：`0x90 0x03 0x92 0x25`
- 成功响应（11字节）：`0x90 0x0A 0x00 0x92 [id×4] [type×2] [chksum]`
- 校验：所有字节（含帧头 0x90）的模 256 累加和

### FreeRTOS 任务

| 任务 | 优先级 | 栈 | 功能 |
|------|--------|-----|------|
| `NfcPollTask` | Normal | 512B | 循环轮询 4 个 NFC 读卡器，更新 `g_nfc_regs[16]` |
| `ModbusTask` | AboveNormal | 512B | USART6 RS485 Modbus 从机，响应 FC=0x04 |
| `defaultTask` | Normal | 512B | 空跑（`osDelay(2000)`） |

共享数据：`g_nfc_regs[16]`（互斥锁 `g_nfc_mutex` 保护）

### DMA 同步信号量

| 信号量 | 用途 | 释放方 |
|--------|------|--------|
| `g_nfc_tx_sem` | NFC 4 路共用 TX 完成（串行轮询无竞争） | `HAL_UART_TxCpltCallback`（USART1/3/4/5） |
| `g_nfc_rx_sem` | NFC 4 路共用 RX 完成 | `HAL_UARTEx_RxEventCallback`（USART1/3/4/5） |
| `g_modbus_tx_sem` | Modbus TX 完成 | `HAL_UART_TxCpltCallback`（USART6） |
| `g_modbus_rx_sem` | Modbus RX 帧接收完成 | `HAL_UARTEx_RxEventCallback`（USART6） |

两个回调均位于 `app_freertos.c`，处理 USART1/3/4/5（NFC）和 USART6（Modbus）；LPUART1 不参与 DMA RX，仅用于 debug TX。

### 用户代码文件

| 文件 | 说明 |
|------|------|
| `Core/Inc/nfc_reader.h` | NFC 接口；`g_nfc_regs`、`g_nfc_mutex`、`g_nfc_tx_sem`、`g_nfc_rx_sem`、`g_nfc_rx_size` extern 声明 |
| `Core/Src/nfc_reader.c` | NFC DMA 轮询逻辑，`NFC_PollAll()`；每轮先 `AbortReceive` 并排空旧 token；含 debug 输出 |
| `Core/Inc/modbus_slave.h` | Modbus 接口，`MODBUS_SLAVE_ADDR` 宏；`g_modbus_rx_sem`、`g_modbus_tx_sem` extern 声明 |
| `Core/Src/modbus_slave.c` | CRC16、FC=0x04 处理、`Modbus_Task()`；TX/RX 均使用 DMA；含 debug 输出 |
| `Core/Inc/debug_uart.h` | Debug 接口：`Debug_Init()`、`Debug_Printf()` |
| `Core/Src/debug_uart.c` | LPUART1 阻塞发送实现，互斥锁保护，FreeRTOS 任务中调用安全 |
| `Core/Src/app_freertos.c` | 任务创建、信号量/debug 初始化；`HAL_UARTEx_RxEventCallback` 和 `HAL_UART_TxCpltCallback` 处理 USART1/3/4/5/6 共 5 路 UART |

## 目录结构

```
slave_control/
├── Core/
│   ├── Inc/          # 头文件（含 nfc_reader.h, modbus_slave.h, debug_uart.h）
│   └── Src/          # 源文件（含 nfc_reader.c, modbus_slave.c, debug_uart.c, app_freertos.c）
├── Drivers/          # STM32 HAL + CMSIS 驱动库
├── Middlewares/      # FreeRTOS 源码
├── MDK-ARM/          # Keil 工程文件及编译产物（.axf / .hex）
└── slave_control.ioc # STM32CubeMX 配置文件（勿手动修改）
```

## 注意事项

- `.ioc` 文件由 STM32CubeMX 管理，**不要手动修改**；重新生成代码时 CubeMX 会覆盖 HAL 初始化部分，用户代码须放在 `USER CODE BEGIN / END` 段中。
- USART1/3/4/5 的 9600 波特率通过 `usart.c` 中 `USER CODE BEGIN USARTx_Init 2` 段的二次 `HAL_UART_Init` 调用实现，**CubeMX 重新生成后需检查该段是否保留**。
- **DMA 模式必须为 `DMA_NORMAL`**：USART1/3/4/5/6 的所有 TX/RX DMA 通道均已配置为 `DMA_NORMAL`。`HAL_UARTEx_ReceiveToIdle_DMA` 和 `HAL_UART_Transmit_DMA` 不支持 `DMA_CIRCULAR` 模式——循环模式下 TC 中断触发后 DMA 立即重启，HAL 状态机混乱，导致收发完全失效。**CubeMX 重新生成后会将 DMA 模式恢复为 CIRCULAR，必须手动改回 NORMAL。**
- Modbus RX 采用 `HAL_UARTEx_ReceiveToIdle_DMA`，利用 UART IDLE 中断检测帧边界；TX 采用 `HAL_UART_Transmit_DMA`，均通过信号量与 `ModbusTask` 同步；回调位于 `app_freertos.c`。
- NFC 轮询使用 `HAL_UART_Transmit_DMA` + `HAL_UARTEx_ReceiveToIdle_DMA`，4 路共用 `g_nfc_tx_sem` / `g_nfc_rx_sem`（串行轮询无竞争）；每个读卡器 RX 超时 300ms，最坏轮询周期约 1.2s。
- **LPUART1 为 debug 输出口**（TX-only，PB11）：使用 `HAL_UART_Transmit` 阻塞发送，不启动 DMA RX，`HAL_UARTEx_RxEventCallback` 中不处理 LPUART1。`Debug_Printf()` 由互斥锁保护，可在任意 FreeRTOS 任务中调用，115200 bps 接 USB-TTL 模块查看日志。
- 所有 DMA RX 启动后立即执行 `__HAL_DMA_DISABLE_IT(..., DMA_IT_HT)` 禁用半满中断，防止 NFC 5 字节超时帧或短 Modbus 帧在缓冲区半满时误触发回调。

## 已修复的 Bug

| 日期 | 文件 | 问题 | 修复 |
|------|------|------|------|
| 2026-05-06 | `usart.c` | USART1/3/4/5/6 所有 DMA 通道配置为 `DMA_CIRCULAR`，导致收发完全失效 | 改为 `DMA_NORMAL` |
| 2026-05-06 | `modbus_slave.c` | CRC 校验通过后多余一次 `HAL_UART_Transmit_DMA` 回显，与后续响应 TX 重叠 | 删除多余发送 |
| 2026-05-06 | `modbus_slave.c` | 每次处理后 `osDelay(1000)` 导致从机 1 秒无响应 | 删除延迟 |
| 2026-05-06 | `usart.c` / `stm32g0xx_it.c` | USART1/3/4/5/6 全局中断未在 NVIC 使能，且缺少 `USART1_IRQHandler` / `USART3_4_5_6_LPUART1_IRQHandler`，导致 DMA TX 完成链中 UART TC 中断无人处理，`gState` 永卡在 `BUSY_TX`，所有 `HAL_UART_Transmit_DMA` 第二次起返回 `HAL_BUSY(2)`，信号量永不释放 | `usart.c` USART1/USART3 Init 2 段加 `HAL_NVIC_SetPriority/EnableIRQ`；`stm32g0xx_it.c` USER CODE 1 段加两个 UART IRQ handler |
| 2026-05-07 | `modbus_slave.c` | RS485 总线有其他设备时，`UART_Start_Receive_DMA` 内部先使能 EIE 再设 DMAR，在此窗口期挂起的 ORE 立即触发中断，调用 `UART_EndRxTransfer`（RxState→READY、ReceptionType→STANDARD）但不 abort DMA（DMAR 尚未设）；`HAL_UARTEx_ReceiveToIdle_DMA` 随后因 ReceptionType 异常返回 HAL_ERROR，DMA 在后台游离运行，下次 `HAL_DMA_Start_IT` 因 DMA state=BUSY 失败，RxState 永卡在 BUSY_RX，串口6收发彻底死锁 | `Modbus_Task` 每次循环顶部先调 `HAL_UART_AbortReceive`（内含清除 ORE/flush RXDR），再排空 `g_modbus_rx_sem`，消除竞争窗口；超时路径不再单独调 AbortReceive（由下次循环顶部统一处理） |

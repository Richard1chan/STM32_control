# STM32G070 4UART 2I2C 项目

## 硬件平台

- **MCU**: STM32G071CBUx（UFQFPN48 封装）
- **系列**: STM32G0
- **系统时钟**: 16 MHz（HSI 内部高速振荡器，不使用 PLL，FLASH_LATENCY_0）
- **工具链**: MDK-ARM V5（Keil uVision）
- **固件库**: STM32Cube FW_G0 V1.4.1
- **CubeMX 版本**: 6.2.1

## 外设配置

### UART（4路）

所有 UART 统一参数：**115200 baud, 8N1, 无硬件流控, 16倍过采样, FIFO 关闭**

| 外设   | HAL 句柄 | TX 引脚 | RX 引脚 | AF        | 波特率  | 备注                           |
|--------|----------|---------|---------|-----------|---------|-------------------------------|
| USART1 | `huart1` | PA9     | PA10    | GPIO_AF1  | 115200  | FIFO 阈值已配置（1/8）后关闭    |
| USART2 | `huart2` | PA2     | PA3     | GPIO_AF1  | 115200  | FIFO 阈值已配置（1/8）后关闭    |
| USART3 | `huart3` | PA5     | PB0     | GPIO_AF4  | **9600**| SC-4M01A 空气质量模块（RX 专用）|
| USART4 | `huart4` | PA0     | PA1     | GPIO_AF4  | **9600**| BA111 EC/TDS 传感器（仅 BOT 板）|

### I2C（2路）

两路参数相同：**Timing=0x00303D5B（约 100kHz 标准模式），7位寻址，模拟滤波器开，数字滤波器=0**

| 外设 | HAL 句柄 | SCL  | SDA  | AF       |
|------|----------|------|------|----------|
| I2C1 | `hi2c1`  | PB6  | PB7  | GPIO_AF6 |
| I2C2 | `hi2c2`  | PB10 | PB11 | GPIO_AF6 |

GPIO 模式：开漏输出 + 内部上拉

#### I2C1 挂载设备

| 设备 | 7位地址 | HAL 地址 | 说明 |
|------|---------|---------|------|
| AHT20-F | 0x38 | 0x70 | 一层温湿度传感器（两板均有） |

#### I2C2 挂载设备

| 设备 | 7位地址 | HAL 地址 | 说明 |
|------|---------|---------|------|
| AHT20-F | 0x38 | 0x70 | 箱外温度传感器（仅 Top 板，BOT 板不接） |
| BH1750FVI | 0x23（ADDR=GND）/ 0x5C（ADDR=VCC） | 0x46 / 0xB8 | 环境光传感器（两板均有） |

### 定时器

| 外设 | HAL 句柄 | 引脚       | 功能                               | 关键参数                                          |
|------|----------|------------|------------------------------------|---------------------------------------------------|
| TIM1 | `htim1`  | PA8（CH1） | PWM 输出（PWM1 模式，高有效）      | PSC=0, ARR=65535, Pulse=0，Break 关闭，Dead Time=0 |
| TIM1 | `htim1`  | PB3（CH2） | PWM 输出（同上）                   | AF: PA8→GPIO_AF2, PB3→GPIO_AF1                    |
| TIM2 | `htim2`  | PA15（CH1）| 外部时钟计数（TI1FP1 上升沿触发）  | PSC=0, ARR=0xFFFFFFFF（32位），从模式 EXTERNAL1   |
| TIM3 | `htim3`  | PA6（CH1） | 外部时钟计数（TI1FP1 上升沿触发）  | PSC=0, ARR=65535（16位），从模式 EXTERNAL1        |
| TIM6 | —        | —          | HAL 系统时基（`HAL_IncTick()`）    | 由 `HAL_TIM_PeriodElapsedCallback` 驱动           |

### FreeRTOS（CMSIS-RTOS V2）

#### 内核参数（FreeRTOSConfig.h）

| 配置项 | 值 | 说明 |
|--------|----|------|
| `configTOTAL_HEAP_SIZE` | 10240 | FreeRTOS 动态堆，10KB |
| `configTICK_RATE_HZ` | 1000 | 1ms tick |
| `configMAX_PRIORITIES` | 56 | CMSIS-RTOS V2 优先级映射要求，不可降低 |
| `configMINIMAL_STACK_SIZE` | 64 words | IDLE 任务栈，256 字节 |
| `configTIMER_TASK_STACK_DEPTH` | 128 words | Timer 守护任务栈，512 字节 |
| `configTIMER_QUEUE_LENGTH` | 10 | 软件定时器命令队列深度 |
| `configQUEUE_REGISTRY_SIZE` | 0 | 队列注册表已关闭（不使用 RTOS 调试视图） |
| `configUSE_TRACE_FACILITY` | 1 | `osThreadEnumerate` 依赖，不可关闭 |
| `configUSE_PORT_OPTIMISED_TASK_SELECTION` | 0 | CM0+ 无 CLZ 指令，必须为 0 |
| 堆实现 | heap_4 | 支持碎片合并，`USE_FreeRTOS_HEAP_4` |

#### 任务列表

| 任务 | 句柄 | 函数 | 优先级 | 栈 | 说明 |
|------|------|------|--------|----|------|
| defaultTask | `defaultTaskHandle` | `StartDefaultTask` | Normal | 1024 B | 备用任务，当前仅 osDelay(1) |
| uart1Task | `uart1TaskHandle` | `StartUart1Task` | Normal | 1024 B | USART1 接收处理 |
| uart2Task | `uart2TaskHandle` | `StartUart2Task` | Normal | 1024 B | USART2 Modbus RTU 从站（RS485，从机地址 0x01） |
| uart3Task | `uart3TaskHandle` | `StartUart3Task` | Normal | 1024 B | SC-4M01A 接收解帧，每10秒打印UART1 |
| uart4Task | `uart4TaskHandle` | `StartUart4Task` | Normal | 1024 B | USART4：BOT板接 BA111 EC传感器，每5秒查询TDS+水温，打印UART1；Top板为通用占位 |
| aht20Task | `aht20TaskHandle` | `StartAht20Task` | Normal | 1024 B | AHT20(I2C1) 一层温湿度 + BH1750(I2C2) 光照 + AHT20(I2C2) 箱外温度（仅Top板），每5秒合并打印UART1 |
| IDLE task | — | 自动创建 | Idle | 256 B | FreeRTOS 内部 |
| Timer task | — | 自动创建 | 2 | 512 B | 软件定时器守护 |

#### 堆内存估算

| 来源 | 消耗 |
|------|------|
| 6 个用户任务（TCB + 栈） | ~6816 B |
| IDLE task | ~380 B |
| Timer task | ~640 B |
| **合计** | **~7836 B** |
| 配置上限 | 10240 B |
| **余量** | **~2404 B** |

#### 任务启动流程

```
main.c:
  UART_DMA_RX_Init()          ← 启动 DMA + 使能 IDLE IRQ（task_handle 尚为 NULL）
  osKernelInitialize()
  MX_FREERTOS_Init()
    osThreadNew(uart1Task~4)   ← 创建任务
    UART_DMA_RX_RegisterTask() ← 注册句柄到驱动
  osKernelStart()              ← 任务开始运行
```

#### UART 任务运行逻辑

```c
for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));  // 阻塞等待 IDLE 通知，10ms 超时兜底
    while ((n = UART_DMA_RX_Read(&huartX, buf, sizeof(buf))) > 0) {
        /* 处理数据 */
    }
}
```

### DMA（4路 UART RX）

| DMA 通道 | 请求源 | 方向 | 模式 | 数据宽度 | 优先级 |
|----------|--------|------|------|----------|--------|
| DMA1_Channel1 | USART1_RX | 外设→内存 | Circular | 字节 | LOW |
| DMA1_Channel2 | USART2_RX | 外设→内存 | Circular | 字节 | LOW |
| DMA1_Channel3 | USART3_RX | 外设→内存 | Circular | 字节 | LOW |
| DMA1_Channel4 | USART4_RX | 外设→内存 | Circular | 字节 | LOW |

### NVIC

| 向量 | 优先级 | 说明 |
|------|--------|------|
| `TIM6_DAC_LPTIM1_IRQn` | 0 | HAL 系统时基 |
| `DMA1_Channel1_IRQn` | 3 | USART1 RX DMA |
| `DMA1_Channel2_3_IRQn` | 3 | USART2/3 RX DMA |
| `DMA1_Ch4_7_DMAMUX1_OVR_IRQn` | 3 | USART4 RX DMA |
| `USART1_IRQn` | 3 | USART1 IDLE 中断（用户代码） |
| `USART2_IRQn` | 3 | USART2 IDLE 中断（用户代码） |
| `USART3_4_LPUART1_IRQn` | 3 | USART3+4 共享 IDLE 中断（用户代码） |
| PendSV / SysTick | 3 | FreeRTOS |

## 代码结构

```
Core/
├── Inc/
│   ├── main.h
│   ├── gpio.h / i2c.h / tim.h / usart.h
│   ├── uart_dma_rx.h                 # DMA+IDLE UART 接收驱动（用户代码）
│   ├── aht20.h                       # AHT20 温湿度传感器驱动（用户代码）
│   ├── bh1750.h                      # BH1750FVI 环境光传感器驱动（用户代码）
│   ├── sc4m01a.h                     # SC-4M01A 空气质量模块驱动（用户代码）
│   ├── modbus_rtu.h                  # Modbus RTU 从站驱动（用户代码）
│   ├── FreeRTOSConfig.h
│   ├── stm32g0xx_hal_conf.h
│   └── stm32g0xx_it.h
└── Src/
    ├── main.c                        # 外设初始化 + RTOS 启动
    ├── uart_dma_rx.c                 # DMA+IDLE UART 接收驱动（用户代码）
    ├── aht20.c                       # AHT20 温湿度传感器驱动（用户代码）
    ├── bh1750.c                      # BH1750FVI 环境光传感器驱动（用户代码）
    ├── sc4m01a.c                     # SC-4M01A 空气质量模块驱动（用户代码）
    ├── modbus_rtu.c                  # Modbus RTU 从站驱动（用户代码）
    ├── app_freertos.c                # FreeRTOS 任务创建
    ├── gpio.c / i2c.c / tim.c / usart.c
    ├── stm32g0xx_it.c                # 中断服务函数（含 UART IDLE ISR）
    ├── stm32g0xx_hal_msp.c           # MSP 低层 GPIO/CLK/DMA 初始化
    ├── stm32g0xx_hal_timebase_tim.c  # TIM6 时基
    └── system_stm32g0xx.c
```

## UART DMA 接收驱动（uart_dma_rx）

### 工作原理

```
UART RX 引脚 → DMA1_ChannelX（Circular）→ dma_buf[256]
                                                   ↓ IDLE 中断
                                             rx_buf[512]（环形）
                                                   ↓ FreeRTOS 任务
                                          UART_DMA_RX_Read()
```

- **DMA Circular 模式**：DMA 持续循环写入 `dma_buf`，无需 CPU 重启传输
- **IDLE 中断**：总线空闲时触发，将 DMA 新写入字节搬到应用层 `rx_buf`
- **SPSC 环形缓冲**：ISR 只写 `rx_wr`，任务只写 `rx_rd`，无需加锁

### API

| 函数 | 说明 |
|------|------|
| `UART_DMA_RX_Init()` | 启动所有通道 DMA 接收并使能 IDLE 中断，在 `MX_USARTx_Init` 后、`osKernelStart` 前调用 |
| `UART_DMA_RX_Read(&huartX, buf, len)` | 从指定通道读取最多 len 字节，返回实际读取数 |
| `UART_DMA_RX_Available(&huartX)` | 返回指定通道可读字节数 |
| `UART_DMA_RX_IdleHandler(&huartX)` | 在 USARTx_IRQHandler 中调用，处理 DMA 新数据 |

### 缓冲区大小

| 宏 | 值 | 说明 |
|----|-----|------|
| `UART_DMA_BUF_SIZE` | 256 | DMA 循环缓冲区（可按需修改，建议 2 的幂） |
| `UART_RX_BUF_SIZE` | 512 | 应用层环形缓冲区 |

### FreeRTOS 任务示例

```c
uint8_t buf[128];
uint16_t n = UART_DMA_RX_Read(&huart1, buf, sizeof(buf));
if (n > 0) {
    // 处理 n 字节数据
}
```

## Modbus RTU 从站驱动（modbus_rtu）

### 概述

- **接口**：USART2（PA2=TX, PA3=RX），115200 bps, 8N1，RS485 总线
- **驱动文件**：`modbus_rtu.h` / `modbus_rtu.c`
- **任务**：`StartUart2Task`，DMA+IDLE 接收帧后调用 `Modbus_Process()`
- **板型选择**：在 Keil 预处理宏中定义 `MODBUS_BOARD_BOT` 切换至 BOT 板（0x02）；不定义则默认 Top 板（0x01）

### 支持功能码

| 功能码 | 说明 |
|--------|------|
| FC=0x04 | Read Input Registers（传感器数据 + PWM 回显，只读） |
| FC=0x06 | Write Single Holding Register（执行器控制） |
| FC=0x10 | Write Multiple Holding Registers（执行器控制） |

### 输入寄存器映射（FC=0x04）

#### 温控板-Top（默认，`MODBUS_SLAVE_ADDR=0x01`，`MODBUS_INPUT_REG_COUNT=9`）

| 地址 | 宏 | 类型 | 比例 | 说明 |
|------|-----|------|------|------|
| 0x0000 | `MODBUS_REG_TEMP1` | INT16 | ×10 | 一层温度（°C×10，由 I2C1 AHT20 写入） |
| 0x0001 | `MODBUS_REG_TEMP2` | INT16 | ×10 | 箱外温度（°C×10，由 I2C2 AHT20 写入） |
| 0x0002 | `MODBUS_REG_HUMID1` | UINT16 | ×10 | 一层湿度（%RH×10，由 I2C1 AHT20 写入） |
| 0x0003 | `MODBUS_REG_CO2_1` | UINT16 | ×1 | 一层CO₂（ppm，由 SC-4M01A 写入） |
| 0x0004 | `MODBUS_REG_LIGHT1` | UINT16 | ×1 | 一层光照（lux，由 BH1750 写入） |
| 0x0005 | — | UINT16 | — | 制冷片1 PWM 回显（镜像 hold[0]） |
| 0x0006 | — | UINT16 | — | 制冷片2 PWM 回显（镜像 hold[1]） |
| 0x0007 | — | UINT16 | — | 风扇1 PWM 回显（镜像 hold[2]） |
| 0x0008 | — | UINT16 | — | 风扇2 PWM 回显（镜像 hold[3]） |

#### 温控板-BOT（`MODBUS_BOARD_BOT`，`MODBUS_SLAVE_ADDR=0x02`，`MODBUS_INPUT_REG_COUNT=8`）

| 地址 | 宏 | 类型 | 比例 | 说明 |
|------|-----|------|------|------|
| 0x0000 | `MODBUS_REG_TEMP1` | INT16 | ×10 | 二层温度（°C×10，由 I2C1 AHT20 写入） |
| 0x0001 | `MODBUS_REG_HUMID1` | UINT16 | ×10 | 二层湿度（%RH×10，由 I2C1 AHT20 写入） |
| 0x0002 | `MODBUS_REG_CO2_1` | UINT16 | ×1 | 二层CO₂（ppm，由 SC-4M01A 写入） |
| 0x0003 | `MODBUS_REG_LIGHT1` | UINT16 | ×1 | 二层光照（lux，由 BH1750 写入） |
| 0x0004 | — | UINT16 | — | 制冷片1 PWM 回显（镜像 hold[0]） |
| 0x0005 | — | UINT16 | — | 制冷片2 PWM 回显（镜像 hold[1]） |
| 0x0006 | — | UINT16 | — | 风扇1 PWM 回显（镜像 hold[2]） |
| 0x0007 | — | UINT16 | — | 风扇2 PWM 回显（镜像 hold[3]） |
| 0x0008 | `MODBUS_REG_EC_ADDR` | UINT16 | ×1 | EC/TDS 值（ppm，由 BA111 通过 UART4 写入 `g_modbus_input_ec`） |

> PWM 回显地址区（`addr >= MODBUS_INPUT_SENSOR_CNT`）在 FC=0x04 处理时直接读取 `g_modbus_hold[addr - MODBUS_INPUT_SENSOR_CNT]`，无需额外存储。

### 保持寄存器映射（FC=0x06 / FC=0x10，两板相同）

| 地址 | 宏 | 类型 | 说明 | PWM 输出 |
|------|-----|------|------|----------|
| 0x0000 | `MODBUS_REG_PWM_COOL1` | UINT16 | 制冷片1 PWM（0~65535） | 未接定时器 |
| 0x0001 | `MODBUS_REG_PWM_COOL2` | UINT16 | 制冷片2 PWM（0~65535） | 未接定时器 |
| 0x0002 | `MODBUS_REG_PWM_FAN1` | UINT16 | 风扇1 PWM（0~65535） | **TIM1_CH1 → PA8** |
| 0x0003 | `MODBUS_REG_PWM_FAN2` | UINT16 | 风扇2 PWM（0~65535） | **TIM1_CH2 → PB3** |

### PWM 输出控制

收到 FC=0x06 或 FC=0x10 写入保持寄存器后，`modbus_rtu.c` 内部立即调用 `apply_pwm()`：

```c
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, g_modbus_hold[MODBUS_REG_PWM_FAN1]);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, g_modbus_hold[MODBUS_REG_PWM_FAN2]);
```

- TIM1 参数：PSC=0，ARR=65535，时钟 16MHz → PWM 频率约 244 Hz
- 占空比 = 寄存器值 / 65535，0=0%，65535=100%
- `Modbus_Init()` 在 `StartUart2Task` 入口调用，执行 `HAL_TIM_PWM_Start` 启动两路输出

### 共享寄存器（跨任务访问）

```c
/* 大小为 MODBUS_INPUT_SENSOR_CNT（Top=5，BOT=4），传感器任务写，Modbus 任务读 */
extern volatile int16_t  g_modbus_input[MODBUS_INPUT_SENSOR_CNT];
/* Modbus 任务写，执行器任务读 */
extern volatile uint16_t g_modbus_hold[MODBUS_HOLD_REG_COUNT];
```

### API

| 函数 | 说明 |
|------|------|
| `Modbus_Init()` | 启动 TIM1 CH1/CH2 PWM 输出，在任务入口调用一次 |
| `Modbus_Process(frame, len)` | 解析一帧 Modbus RTU 请求并发送响应，写寄存器后自动更新 PWM |

---

## SC-4M01A 六合一空气质量监测模块

### 模块概述

型号 SC-4M01A，深圳市深晨科技，UART 主动上报，同时输出 6 路数据。

> **注意**：CO₂ 和 HCHO 数值为 TVOC 经软件运算模拟的等效值，仅供参考。

### 测量参数及量程

| 参数 | 量程 | 单位 |
|------|------|------|
| TVOC（总有机挥发物） | 0 ~ 2000 | μg/m³ |
| CO₂（等效值） | 400 ~ 5000 | ppm |
| HCHO（甲醛，等效值） | 0 ~ 1000 | μg/m³ |
| 温度 | 0 ~ 130（建议 0~50） | °C |
| 湿度 | 0 ~ 100 | %RH |
| AQI | 1 ~ 6 | — |

### 硬件接口

| 端口 | 功能 |
|------|------|
| 端口1 | GND（电源负极） |
| 端口2 | VCC（3.3V 或 5V） |
| 端口3 | A（RX，接 MCU TX） |
| 端口4 | B（TX，接 MCU RX） |

供电：3.3V/5V DC，工作电流 ≤25mA，预热时间 60 秒。

### UART 通信协议

**串口参数：9600 bps, 8N1, 无校验**（与项目其他 UART 的 115200 不同，接入时需单独配置该通道波特率）

- 模块每 **1 秒**主动上报一帧，共 **14 字节**
- 帧格式：`B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 B10 B11 B12 CHECKSUM`

### 数据帧解析

| 字节 | 含义 | 计算公式 | 示例 |
|------|------|----------|------|
| B0 B1 | 帧头（固定） | `0x2C 0xE4` | — |
| B2 B3 | TVOC | `(B3×256 + B2) × 0.001` mg/m³ = μg/m³ 整数值 | B3=1,B2=133 → 389 μg/m³ |
| B4 B5 | HCHO（甲醛） | `(B5×256 + B4) × 0.001` mg/m³ = μg/m³ 整数值 | B5=0,B4=111 → 111 μg/m³ |
| B6 B7 | CO₂（等效） | `B7×256 + B6`（ppm） | B7=2,B6=207 → 719 ppm |
| B8 | AQI | `B8`（1~6） | 3 |
| B9 B10 | 温度 | `B10 + B9÷10`（°C） | B10=25,B9=6 → 25.6°C |
| B11 B12 | 湿度 | `B12 + B11÷10`（%RH） | B12=45,B11=4 → 45.4%RH |
| CHECKSUM | 校验 | `(B0+…+B12) & 0xFF` | — |

### 接入信息（BA111）

- **已接入 USART4**（PA0=TX, PA1=RX），**仅 BOT 板**，波特率已改为 9600 bps
- 驱动文件：`ba111.h` / `ba111.c`
- 任务：`StartUart4Task`（BOT 编译时启用），每 5 秒主动发送检测指令，等待 IDLE 中断后解析响应
- 输出格式（UART1，每 5 秒）：`[EC] TDS=100 ppm  T=27.1 C`
- TDS 值（ppm）写入 Modbus 输入寄存器 `g_modbus_input_ec`，FC=0x04 地址 0x0008

---

### SC-4M01A 接入信息

- **已接入 USART3**（PA5=TX, PB0=RX），波特率已改为 9600 bps
- 驱动文件：`sc4m01a.h` / `sc4m01a.c`，提供有状态解析器 `SC4M01A_Parser_t`
- 任务：`StartUart3Task`，持续接收解帧，**每 10 秒**通过 UART1 输出一行：

```
[AQI] TVOC=389 ug/m3  HCHO=111 ug/m3  CO2=719 ppm  AQI=3  T=25.6 C  RH=45.4%
```

- 帧解析器特性：跨帧容错（自动丢弃乱码字节，定位帧头后校验 CHECKSUM）

---

## BH1750FVI 环境光驱动（bh1750）

### 工作原理

- 上电后发 **Power On**（0x01）+ **Continuously H-Resolution Mode**（0x10）命令
- 传感器以 1lx 分辨率、约 120ms 周期持续自动测量，无需每次重新触发
- 读取时直接 I2C 读 2 字节，转换公式：`lux = raw_16bit / 1.2`（整数近似：`raw × 5 / 6`）

### API

| 函数 | 说明 |
|------|------|
| `BH1750_Init(hi2c, addr)` | 上电 + 启动连续 H-Resolution 模式，内部 osDelay(180ms) |
| `BH1750_Read(hi2c, addr, &lux)` | 读取最新光照度（lx），连续模式下随时可读 |

### 地址宏

| 宏 | 值（HAL 8位） | ADDR 引脚 |
|----|-------------|----------|
| `BH1750_ADDR_L` | 0x46 | GND（≤0.3VCC） |
| `BH1750_ADDR_H` | 0xB8 | VCC（≥0.7VCC） |

### 输出格式（UART1，每 5 秒）

```
[ENV] T=25.3 C  RH=60.2%  Lux=1234
```

---

## BA111 TDS/水温传感器芯片

### 芯片概述

型号 BA111，Atombit（atombit.cn），SOP8 封装。内部集成高精密振荡电路、模数转换电路和浮点运算单元，采用 Atombit® 专利电导率-TDS 转换算法和温度校正算法。

> 属于 EC（电导率）传感方案，内部将电导率转换为 TDS 输出。

### 测量参数及量程

| 参数 | 量程 | 精度 |
|------|------|------|
| TDS（总溶解性固体） | 0 ~ 3000 ppm | <2% F.S. |
| 水温 | 0 ~ 100 °C | ±0.5 °C |

### 电气特性

- 供电：3.3V（电源纹波 <20mV）；最高可接 5V，但检测结果需乘以系数 **1.52** 修正
- 工作电流：<3mA
- 封装：SOP8

### 引脚说明

| 引脚 | 符号 | 说明 |
|------|------|------|
| 1 | VDD | 3.3V 供电 |
| 2 | TDS1-ACT1 | 探针驱动信号，接 TDS 探针 1 |
| 3 | TDS1-ACT2 | 探针驱动信号，通过 1% 精度电阻接 TDS 探针 2 |
| 4 | UART-RXD | 接 MCU TX |
| 5 | UART-TXD | 接 MCU RX |
| 6 | TDS1-AD | TDS 模拟信号输入 |
| 7 | TDS1-NTC | 温度 NTC 信号输入 |
| 8 | GND | 电源地 |

> 3.3V 供电时，UART 引脚（4、5 脚）最高耐压 3.3+0.3V，超过则永久损坏。

### UART 通信

**串口参数：9600 bps, 8N1, 无校验**

- 帧格式：`命令(1B) + 参数(4B) + 校验和(1B)` = 固定 **6 字节**
- **校验和算法：前 5 字节逐字节累加，取低 8 位（& 0xFF）**
  - 即：`checksum = (B0 + B1 + B2 + B3 + B4) & 0xFF`
  - 注：数据手册检测响应示例中校验和 `0x40` 疑为印刷错误，按算法应为 `0xAE`

### 指令说明

#### 1. 检测指令（读取 TDS + 水温）

```
发送：A0 00 00 00 00 A0
响应：AA TDS_H TDS_L TEMP_H TEMP_L checksum
```

数据解析：
- TDS（ppm）= `(TDS_H << 8) | TDS_L`（直接读取，已含温度校正）
- 水温（°C）= `((TEMP_H << 8) | TEMP_L) / 100.0f`

示例：`AA 00 64 0A 96 [checksum]`
- TDS = 0x0064 = **100 ppm**
- 温度 = 0x0A96 / 100 = **27.1 °C**

EC传感器测量的是TDS ，单位是PPM，需要换算成EC值，初步选用自来水的系数0.5，后续更改。
TDS = K × EC₂₅   如果自来水测量 100PPM， EC=100/0.5=200 us/cm

#### 2. 基线校准

```
发送：A6 00 00 00 00 A6
成功响应：AC 00 00 00 00 AC
```
> 执行前须将探头放入 **25°C ± 5°C 纯净水**中

#### 3. 设置 NTC 常温电阻值

```
发送：A3 NTC_B3 NTC_B2 NTC_B1 NTC_B0 checksum
成功响应：AC 00 00 00 00 AC
```
示例：`A3 00 01 86 A0 CA`（NTC = 0x000186A0 = 100000Ω）

#### 4. 设置 NTC B 值

```
发送：A5 00 00 B_H B_L checksum
成功响应：AC 00 00 00 00 AC
```
示例：`A5 0F 0A 00 00 BE`（B = 0x0F0A = 3850）

#### 异常响应格式

```
AC XX 00 00 00 checksum
```

| 异常码 XX | 说明 |
|-----------|------|
| 0x01 | 命令帧异常 |
| 0x02 | 忙碌中 |
| 0x03 | 校正失败 |
| 0x04 | 检测温度超出范围 |

### 默认 NTC 参数

| 参数 | 默认值 |
|------|--------|
| 常温电阻值 | 10 KΩ |
| B 值 | 3435 |

> R2 分压电阻阻值须与所用 NTC 探头常温阻值相同，否则温度补偿失效。

### 注意事项

- 不接 NTC 时芯片可正常输出 TDS，但无温度校正效果
- 两个 TDS 探头测量同一水体时相互干扰，探针间距不小于 **1 米**
- 建议使用隔离电源以提高复杂电磁环境下的稳定性

---

## 当前状态

- DMA+IDLE UART 接收驱动已实现
- AHT20（温湿度）分挂两路 I2C：I2C1（一层温湿度，两板均有）和 I2C2（箱外温度，仅 Top 板，BOT 板不接）；BH1750FVI（光照）挂在 I2C2；由 `StartAht20Task` 每 5 秒采集并合并输出至 UART1；同步将数据写入 `g_modbus_input` 供 Modbus 读取
- SC-4M01A 空气质量模块接 USART3（9600 bps），由 `StartUart3Task` 持续解帧，每 10 秒输出至 UART1；CO₂ 数据同步写入 `g_modbus_input[MODBUS_REG_CO2_1]`
- USART3 波特率已在 `usart.c` 改为 9600，若 CubeMX 重新生成需手动恢复
- Modbus RTU 从站已实现（USART2），支持 FC=0x04/0x06/0x10；收到写保持寄存器命令后立即更新 TIM1 PWM 输出
- 默认编译为 **温控板-Top**（从机地址 0x01）；在 Keil 预处理宏中加入 `MODBUS_BOARD_BOT` 可切换为 **温控板-BOT**（从机地址 0x02）
- FC=0x04 输入寄存器：Top 共 9 个（5 个传感器 + 4 个 PWM 回显），BOT 共 8 个（4 个传感器 + 4 个 PWM 回显）；PWM 回显由驱动实时从 `g_modbus_hold` 读取，无需额外存储
- BOT 模式下 `StartAht20Task` 跳过 I2C2 AHT20 初始化和 `MODBUS_REG_TEMP2` 写入（BOT 板箱外温度传感器不接）
- BOT 模式下 `StartUart4Task` 接管 BA111 EC传感器采集：每 5 秒发送检测指令 `A0 00 00 00 00 A0`，解析 TDS（ppm）和水温（°C×10），写入 `g_modbus_input_ec`（FC=0x04 地址 0x0008），并通过 UART1 打印 `[EC] TDS=xxx ppm  T=xx.x C`
- USART4 波特率已在 `usart.c` 改为 9600（BA111 要求），若 CubeMX 重新生成需手动恢复
- TIM1_CH1（PA8）受保持寄存器 0x0002（`MODBUS_REG_PWM_FAN1`）控制，TIM1_CH2（PB3）受 0x0003（`MODBUS_REG_PWM_FAN2`）控制，范围 0~65535
- `defaultTask` 目前仅 `osDelay(1)`，业务逻辑应添加在各 `/* USER CODE BEGIN ... */` 区域内以防 CubeMX 重新生成时被覆盖

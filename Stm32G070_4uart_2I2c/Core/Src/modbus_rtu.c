/**
 * @file    modbus_rtu.c
 * @brief   Modbus RTU Slave — 温控板 (USART2 / RS485)
 *          板型由 modbus_rtu.h 中的编译开关决定（Top=默认 / BOT=MODBUS_BOARD_BOT）
 */

#include "modbus_rtu.h"
#include "usart.h"
#include "tim.h"

/* ── 共享寄存器定义 ──────────────────────────────────────────────────── */
/* 仅存放传感器数据，PWM 回显在 FC=0x04 处理时从 g_modbus_hold 读取 */
volatile int16_t  g_modbus_input[MODBUS_INPUT_SENSOR_CNT] = {0};
volatile uint16_t g_modbus_hold[MODBUS_HOLD_REG_COUNT]    = {0};
#if defined(MODBUS_BOARD_BOT)
/* EC 传感器寄存器（地址 0x0008），由 uart4Task（BA111）写入 */
volatile uint16_t g_modbus_input_ec = 0u;
#endif

/* ── CRC16 (Modbus RTU, poly=0xA001) ────────────────────────────────── */
static uint16_t crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--) {
        crc ^= (uint16_t)(*buf++);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001u)
                crc = (crc >> 1) ^ 0xA001u;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ── PWM 输出应用（hold[2]→TIM1_CH1/PA8，hold[3]→TIM1_CH2/PB3）────── */
static void apply_pwm(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, g_modbus_hold[MODBUS_REG_PWM_FAN1]);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, g_modbus_hold[MODBUS_REG_PWM_FAN2]);
}

/* ── RS485 发送 ──────────────────────────────────────────────────────── */
static void rs485_send(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 50u);
}

/* ── 发送异常响应 ─────────────────────────────────────────────────────── */
static void send_exception(uint8_t fc, uint8_t exc_code)
{
    uint8_t rsp[5];
    rsp[0] = MODBUS_SLAVE_ADDR;
    rsp[1] = fc | 0x80u;
    rsp[2] = exc_code;
    uint16_t crc = crc16(rsp, 3u);
    rsp[3] = (uint8_t)(crc & 0xFFu);
    rsp[4] = (uint8_t)(crc >> 8u);
    rs485_send(rsp, 5u);
}

/* ── 公开 API ────────────────────────────────────────────────────────── */

void Modbus_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
}

void Modbus_Process(const uint8_t *frame, uint16_t len)
{
    /* 最短有效帧：地址(1)+功能码(1)+CRC(2) = 4 字节 */
    if (len < 4u) return;

    /* 非本机地址，忽略 */
    if (frame[0] != MODBUS_SLAVE_ADDR) return;

    /* CRC 校验 */
    uint16_t crc_calc = crc16(frame, (uint16_t)(len - 2u));
    uint16_t crc_recv = (uint16_t)frame[len - 2u] | ((uint16_t)frame[len - 1u] << 8u);
    if (crc_calc != crc_recv) return;

    uint8_t fc = frame[1];

    /* ── FC=0x04  Read Input Registers ──────────────────────────────── */
    if (fc == 0x04u) {
        /* 请求格式：[addr][04][start_hi][start_lo][qty_hi][qty_lo][crc×2] = 8 B */
        if (len < 8u) { send_exception(fc, 0x03u); return; }

        uint16_t start = ((uint16_t)frame[2] << 8u) | frame[3];
        uint16_t qty   = ((uint16_t)frame[4] << 8u) | frame[5];

        if (qty == 0u || qty > 125u)              { send_exception(fc, 0x03u); return; }
        if (start + qty > MODBUS_INPUT_REG_COUNT) { send_exception(fc, 0x02u); return; }

        /* 响应：[addr][04][byte_cnt][data...][crc×2] */
        uint8_t rsp[3u + 2u * MODBUS_INPUT_REG_COUNT + 2u];
        uint8_t byte_cnt = (uint8_t)(qty * 2u);
        rsp[0] = MODBUS_SLAVE_ADDR;
        rsp[1] = fc;
        rsp[2] = byte_cnt;

        for (uint16_t i = 0u; i < qty; i++) {
            uint16_t idx = start + i;
            uint16_t val;
            if (idx < MODBUS_INPUT_SENSOR_CNT) {
                /* 传感器数据区（0x0000 ~ SENSOR_CNT-1） */
                val = (uint16_t)g_modbus_input[idx];
#if defined(MODBUS_BOARD_BOT)
            } else if (idx == MODBUS_REG_EC_ADDR) {
                /* EC 传感器寄存器（BOT 专用，固定地址 0x0008） */
                val = g_modbus_input_ec;
#endif
            } else {
                /* PWM 回显区：映射到对应保持寄存器 */
                val = g_modbus_hold[idx - MODBUS_INPUT_SENSOR_CNT];
            }
            rsp[3u + i * 2u] = (uint8_t)(val >> 8u);
            rsp[4u + i * 2u] = (uint8_t)(val & 0xFFu);
        }

        uint16_t rsp_len = 3u + byte_cnt;
        uint16_t crc = crc16(rsp, rsp_len);
        rsp[rsp_len]      = (uint8_t)(crc & 0xFFu);
        rsp[rsp_len + 1u] = (uint8_t)(crc >> 8u);
        rs485_send(rsp, (uint16_t)(rsp_len + 2u));

    /* ── FC=0x06  Write Single Holding Register ──────────────────────── */
    } else if (fc == 0x06u) {
        /* 请求格式：[addr][06][reg_hi][reg_lo][val_hi][val_lo][crc×2] = 8 B */
        if (len < 8u) { send_exception(fc, 0x03u); return; }

        uint16_t reg = ((uint16_t)frame[2] << 8u) | frame[3];
        uint16_t val = ((uint16_t)frame[4] << 8u) | frame[5];

        if (reg >= MODBUS_HOLD_REG_COUNT) { send_exception(fc, 0x02u); return; }

        g_modbus_hold[reg] = val;
        apply_pwm();

        /* 标准应答：原样回显请求帧（含 CRC） */
        rs485_send(frame, len);

    /* ── FC=0x10  Write Multiple Holding Registers ───────────────────── */
    } else if (fc == 0x10u) {
        /* 请求格式：[addr][10][start_hi][start_lo][qty_hi][qty_lo][byte_cnt][data...][crc×2] */
        if (len < 9u) { send_exception(fc, 0x03u); return; }

        uint16_t start    = ((uint16_t)frame[2] << 8u) | frame[3];
        uint16_t qty      = ((uint16_t)frame[4] << 8u) | frame[5];
        uint8_t  byte_cnt = frame[6];

        if (qty == 0u || qty > 123u)                 { send_exception(fc, 0x03u); return; }
        if (byte_cnt != (uint8_t)(qty * 2u))         { send_exception(fc, 0x03u); return; }
        if (start + qty > MODBUS_HOLD_REG_COUNT)     { send_exception(fc, 0x02u); return; }
        if (len < (uint16_t)(7u + byte_cnt + 2u))   { send_exception(fc, 0x03u); return; }

        for (uint16_t i = 0u; i < qty; i++) {
            g_modbus_hold[start + i] =
                ((uint16_t)frame[7u + i * 2u] << 8u) | frame[8u + i * 2u];
        }
        apply_pwm();

        /* 响应：[addr][10][start_hi][start_lo][qty_hi][qty_lo][crc×2] = 8 B */
        uint8_t rsp[8];
        rsp[0] = MODBUS_SLAVE_ADDR;
        rsp[1] = fc;
        rsp[2] = frame[2];
        rsp[3] = frame[3];
        rsp[4] = frame[4];
        rsp[5] = frame[5];
        uint16_t crc = crc16(rsp, 6u);
        rsp[6] = (uint8_t)(crc & 0xFFu);
        rsp[7] = (uint8_t)(crc >> 8u);
        rs485_send(rsp, 8u);

    } else {
        /* 不支持的功能码 */
        send_exception(fc, 0x01u);
    }
}

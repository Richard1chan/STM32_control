#include "modbus_slave.h"
#include "nfc_reader.h"
#include "debug_uart.h"
#include <string.h>

osSemaphoreId_t  g_modbus_rx_sem;
osSemaphoreId_t  g_modbus_tx_sem;
uint8_t          g_modbus_rx_buf[MODBUS_RX_BUF_SIZE];
volatile uint16_t g_modbus_rx_len;

/* Standard Modbus CRC16 (polynomial 0xA001, little-endian output) */
uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static void Send_ExceptionResponse(uint8_t func_code, uint8_t exception_code)
{
    uint8_t resp[5];
    resp[0] = MODBUS_SLAVE_ADDR;
    resp[1] = func_code | 0x80;
    resp[2] = exception_code;
    uint16_t crc = Modbus_CRC16(resp, 3);
    resp[3] = (uint8_t)(crc & 0xFF);
    resp[4] = (uint8_t)(crc >> 8);
    HAL_UART_Transmit_DMA(&huart6, resp, 5);
    osSemaphoreAcquire(g_modbus_tx_sem, 100);
}

static void Process_FC04(const uint8_t *frame)
{
    uint16_t start_addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t reg_count  = ((uint16_t)frame[4] << 8) | frame[5];

    /* Validate register range: 0x0000 to 0x000F (16 registers) */
    if (reg_count == 0 || reg_count > NFC_TOTAL_REGS ||
        start_addr >= NFC_TOTAL_REGS ||
        (start_addr + reg_count) > NFC_TOTAL_REGS)
    {
        Debug_Printf("[MB] FC04 illegal addr=%04X cnt=%u\r\n", start_addr, reg_count);
        Send_ExceptionResponse(0x04, 0x02); /* Illegal Data Address */
        return;
    }

    /* Response: addr + FC + byte_count + data + CRC */
    uint8_t  resp[3 + NFC_TOTAL_REGS * 2 + 2];
    uint8_t  byte_count = (uint8_t)(reg_count * 2);
    uint16_t resp_len   = 3u + byte_count + 2u;

    resp[0] = MODBUS_SLAVE_ADDR;
    resp[1] = 0x04;
    resp[2] = byte_count;

    osMutexAcquire(g_nfc_mutex, osWaitForever);
    for (uint16_t i = 0; i < reg_count; i++) {
        uint16_t val = g_nfc_regs[start_addr + i];
        resp[3 + i * 2]     = (uint8_t)(val >> 8);
        resp[3 + i * 2 + 1] = (uint8_t)(val & 0xFF);
    }
    osMutexRelease(g_nfc_mutex);

    uint16_t crc = Modbus_CRC16(resp, resp_len - 2u);
    resp[resp_len - 2] = (uint8_t)(crc & 0xFF);
    resp[resp_len - 1] = (uint8_t)(crc >> 8);

    Debug_Printf("[MB] FC04 addr=%04X cnt=%u -> TX %u bytes\r\n", start_addr, reg_count, resp_len);
    HAL_UART_Transmit_DMA(&huart6, resp, resp_len);
    osSemaphoreAcquire(g_modbus_tx_sem, 100);
}

void Modbus_Init(void)
{
    g_modbus_rx_len = 0;
}

void Modbus_Task(void *arg)
{
    (void)arg;

    Debug_Printf("[MB] Modbus task started, addr=0x%02X\r\n", MODBUS_SLAVE_ADDR);

    for (;;) {
        /* Abort any leftover DMA and clear stale ORE/errors before arming receiver.
         * On a busy RS485 bus, pending ORE fires the moment EIE is re-enabled inside
         * UART_Start_Receive_DMA (before DMAR is set), resetting ReceptionType without
         * aborting the DMA — leaving hdma stuck BUSY and RxState stuck BUSY_RX. */
        HAL_UART_AbortReceive(&huart6);
        osSemaphoreAcquire(g_modbus_rx_sem, 0);

        HAL_StatusTypeDef rx_start = HAL_UARTEx_ReceiveToIdle_DMA(&huart6, g_modbus_rx_buf, MODBUS_RX_BUF_SIZE);
        if (rx_start != HAL_OK) {
            Debug_Printf("[MB] DMA start FAIL=%d RxState=%d\r\n", rx_start, (int)huart6.RxState);
            osDelay(10);
            continue;
        }
        __HAL_DMA_DISABLE_IT(huart6.hdmarx, DMA_IT_HT);

        osStatus_t rx_sem = osSemaphoreAcquire(g_modbus_rx_sem, 5000);
        if (rx_sem != osOK) {
            Debug_Printf("[MB] RX TIMEOUT ISR=%08lX CR1=%08lX\r\n",
                         (unsigned long)USART6->ISR, (unsigned long)USART6->CR1);
            continue;
        }

        uint16_t len = g_modbus_rx_len;

        Debug_Printf("[MB] RX len=%u", len);
        for (uint16_t i = 0; i < len && i < 8; i++) {
            Debug_Printf(" %02X", g_modbus_rx_buf[i]);
        }
        Debug_Printf("\r\n");

        /* Minimum valid Modbus RTU frame is 8 bytes (FC=0x04 request) */
        if (len < 8) {
            Debug_Printf("[MB] Frame too short (%u)\r\n", len);
            continue;
        }

        /* Check slave address */
        if (g_modbus_rx_buf[0] != MODBUS_SLAVE_ADDR) {
            Debug_Printf("[MB] Addr mismatch: got %02X, expect %02X\r\n",
                         g_modbus_rx_buf[0], MODBUS_SLAVE_ADDR);
            continue;
        }

        /* Verify CRC16 */
        uint16_t received_crc = ((uint16_t)g_modbus_rx_buf[len - 1] << 8) |
                                  g_modbus_rx_buf[len - 2];
        uint16_t computed_crc = Modbus_CRC16(g_modbus_rx_buf, len - 2u);
        if (received_crc != computed_crc) {
            Debug_Printf("[MB] CRC ERR: got %04X, calc %04X\r\n", received_crc, computed_crc);
            continue;
        }

        /* Dispatch by function code */
        uint8_t fc = g_modbus_rx_buf[1];
        Debug_Printf("[MB] FC=%02X OK\r\n", fc);
        if (fc == 0x04) {
            Process_FC04(g_modbus_rx_buf);
        } else {
            Send_ExceptionResponse(fc, 0x01); /* Illegal Function */
        }
    }
}

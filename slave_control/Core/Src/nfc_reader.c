#include "nfc_reader.h"
#include "debug_uart.h"
#include <string.h>

/* NFC522-V3.1 read ID command: 0x90 0x03 0x92, checksum=(0x90+0x03+0x92)%256 */
static const uint8_t NFC_CMD_READ_ID[4] = {0x90, 0x03, 0x92, 0x25};

/* UART handles for 4 NFC readers */
static UART_HandleTypeDef * const NFC_UARTS[NFC_READER_COUNT] = {
    &huart1,  /* READER1: PC4/PC5  */
    &huart3,  /* READER2: PB2/PD9  */
    &huart4,  /* READER3: PA0/PC11 */
    &huart5,  /* READER4: PC12/PD2 */
};

uint16_t         g_nfc_regs[NFC_TOTAL_REGS];
osMutexId_t      g_nfc_mutex;
osSemaphoreId_t  g_nfc_tx_sem;
osSemaphoreId_t  g_nfc_rx_sem;
volatile uint16_t g_nfc_rx_size;

/* Verify NFC522 response checksum (MOD 256 of all bytes including 0x90 header) */
static uint8_t NFC_Checksum(const uint8_t *buf, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += buf[i];
    }
    return sum;
}

static void NFC_Poll(UART_HandleTypeDef *huart, uint8_t reader_idx)
{
    uint8_t rx_buf[16];
    uint16_t base = (uint16_t)reader_idx * NFC_REGS_PER_READER;

    /* Abort any stale DMA RX and drain leftover semaphore tokens */
    HAL_UART_AbortReceive(huart);
    osSemaphoreAcquire(g_nfc_rx_sem, 0);
    osSemaphoreAcquire(g_nfc_tx_sem, 0);

    /* Send read command via DMA TX */
    HAL_StatusTypeDef tx_ret = HAL_UART_Transmit_DMA(huart, (uint8_t *)NFC_CMD_READ_ID, sizeof(NFC_CMD_READ_ID));
    osStatus_t tx_ok = osSemaphoreAcquire(g_nfc_tx_sem, 50);
    Debug_Printf("[NFC%d] TX ret=%d sem=%d\r\n", reader_idx + 1, tx_ret, tx_ok);

    /*
     * Success response is 11 bytes: 0x90 0x0A 0x00 0x92 id[4] type[2] chksum
     * Timeout response is  5 bytes: 0x90 0x04 0x04 0x92 chksum
     * IDLE event fires when the module stops transmitting (5 or 11 bytes).
     * HT interrupt is disabled to avoid spurious wake on the 5-byte no-card frame.
     */
    HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, 11);
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);

    osStatus_t rx_ok  = osSemaphoreAcquire(g_nfc_rx_sem, 300);
    uint16_t   rx_len = g_nfc_rx_size;
    if (rx_ok != osOK) {
        HAL_UART_AbortReceive(huart);
        Debug_Printf("[NFC%d] RX TIMEOUT\r\n", reader_idx + 1);
    } else {
        Debug_Printf("[NFC%d] RX len=%u data:", reader_idx + 1, rx_len);
        for (uint16_t i = 0; i < rx_len; i++) {
            Debug_Printf(" %02X", rx_buf[i]);
        }
        Debug_Printf("\r\n");
    }

    osMutexAcquire(g_nfc_mutex, osWaitForever);

    if (rx_ok == osOK && rx_len == 11 &&
        rx_buf[0] == 0x90 &&
        rx_buf[2] == 0x00 &&   /* status: success */
        rx_buf[3] == 0x92 &&   /* echo of command */
        NFC_Checksum(rx_buf, 10) == rx_buf[10])
    {
        /* id[0..3] at buf[4..7], type[0..1] at buf[8..9] */
        g_nfc_regs[base + 0] = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];
        g_nfc_regs[base + 1] = ((uint16_t)rx_buf[6] << 8) | rx_buf[7];
        g_nfc_regs[base + 2] = ((uint16_t)rx_buf[8] << 8) | rx_buf[9];
        g_nfc_regs[base + 3] = 0x0000;
        Debug_Printf("[NFC%d] Card ID=%04X%04X type=%04X\r\n",
                     reader_idx + 1,
                     g_nfc_regs[base + 0], g_nfc_regs[base + 1],
                     g_nfc_regs[base + 2]);
    }
    else
    {
        /* No card, timeout, or communication error - clear registers */
        g_nfc_regs[base + 0] = 0;
        g_nfc_regs[base + 1] = 0;
        g_nfc_regs[base + 2] = 0;
        g_nfc_regs[base + 3] = 0;
    }

    osMutexRelease(g_nfc_mutex);
}

void NFC_Init(void)
{
    memset(g_nfc_regs, 0, sizeof(g_nfc_regs));
}

void NFC_PollAll(void)
{
    for (uint8_t i = 0; i < NFC_READER_COUNT; i++) {
        NFC_Poll(NFC_UARTS[i], i);
    }
}

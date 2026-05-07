#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include "usart.h"
#include "cmsis_os.h"
#include <stdint.h>

/*
 * Modbus slave address.
 * Board handling READER1-4: 0x0A
 * Board handling READER5-8: 0x0B
 */
#define MODBUS_SLAVE_ADDR  0x0A

/* RX buffer size - must fit the largest expected request (8 bytes) with margin */
#define MODBUS_RX_BUF_SIZE  64

extern osSemaphoreId_t   g_modbus_rx_sem;
extern osSemaphoreId_t   g_modbus_tx_sem;
extern uint8_t           g_modbus_rx_buf[MODBUS_RX_BUF_SIZE];
extern volatile uint16_t g_modbus_rx_len;

void     Modbus_Init(void);
void     Modbus_Task(void *arg);
uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len);

#endif /* MODBUS_SLAVE_H */

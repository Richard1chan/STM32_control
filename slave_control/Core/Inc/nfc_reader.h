#ifndef NFC_READER_H
#define NFC_READER_H

#include "usart.h"
#include "cmsis_os.h"
#include <stdint.h>

/* 4 readers, 4 registers each = 16 registers total */
#define NFC_READER_COUNT    4
#define NFC_REGS_PER_READER 4
#define NFC_TOTAL_REGS      (NFC_READER_COUNT * NFC_REGS_PER_READER)

/* Shared register buffer - protected by g_nfc_mutex */
extern uint16_t      g_nfc_regs[NFC_TOTAL_REGS];
extern osMutexId_t   g_nfc_mutex;

/* DMA synchronization - signaled from HAL callbacks */
extern osSemaphoreId_t   g_nfc_tx_sem;
extern osSemaphoreId_t   g_nfc_rx_sem;
extern volatile uint16_t g_nfc_rx_size;

void NFC_Init(void);
void NFC_PollAll(void);

#endif /* NFC_READER_H */

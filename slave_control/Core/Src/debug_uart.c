#include "debug_uart.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdarg.h>

static osMutexId_t s_debug_mutex;

void Debug_Init(void)
{
    s_debug_mutex = osMutexNew(NULL);
}

/* Thread-safe blocking debug print via LPUART1 (115200, no DMA) */
void Debug_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len <= 0) return;

    if (s_debug_mutex) {
        osMutexAcquire(s_debug_mutex, osWaitForever);
    }
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)buf, (uint16_t)len, 200);
    if (s_debug_mutex) {
        osMutexRelease(s_debug_mutex);
    }
}

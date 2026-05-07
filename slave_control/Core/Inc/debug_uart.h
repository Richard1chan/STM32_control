#ifndef DEBUG_UART_H
#define DEBUG_UART_H

/* Debug output via LPUART1 (PB11=TX, PB10=RX), 115200 bps, blocking transmit */

void Debug_Init(void);
void Debug_Printf(const char *fmt, ...);

#endif /* DEBUG_UART_H */

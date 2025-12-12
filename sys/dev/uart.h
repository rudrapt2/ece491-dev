/*! @file uart.h
    @brief NS8250-compatible serial port
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _UART_H_
#define _UART_H_

#include "device.h" // for struct serial

extern void attach_uart(void * mmio_base, int irqno);

#endif // _UART_H_
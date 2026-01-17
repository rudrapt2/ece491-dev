// uart.h - NS8550-compatible UART
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _UART_H_
#define _UART_H_

extern void attach_uart(void * mmio_base, int irqno);

#endif // _UART_H_
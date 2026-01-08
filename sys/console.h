/*! @file console.h
    @brief Console i/o
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stddef.h>
#include <stdarg.h>

#include "see.h"

extern char console_initialized;

extern void console_init(void);

extern void kputc(char c);

extern char kgetc(void);

extern void kputs(const char * str);

extern char * kgetsn(char * buf, size_t n);

extern void kprintf(const char * fmt, ...);

extern void kvprintf(const char * fmt, va_list ap);

#endif // _CONSOLE_H_

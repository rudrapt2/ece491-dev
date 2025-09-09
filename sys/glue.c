/*! @file glue.c
    @brief Glue code for Super Star Trek
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#include <stdarg.h>
#include <stddef.h>
#include <io.h>

/**
* @brief pseudo random number generator for trek
* @return random integer
*/
static int rand(void);

/**
* @brief evaluates the absolute value of an integer
* @param x input
* @return -x if x<0, otherwise x
*/
static int abs(int x);

/**
* @brief ascii to integer
* @param s string to convert
* @return integer representation
*/
static int atoi(const char *s);

/**
* @brief copies s1 to s0
* @param s0 string copied to
* @param s1 string to copy
* @return copied string (s0)
*/
static char * strcpy(char * s0, const char * s1);

/**
* @brief appends s1 onto s0
* @param s0 first part of concat
* @param s1 second part of concat
* @return concatenated string (s0)
*/
static char * strcat(char * s0, const char * s1);

static int exited; // 1 if user is trying to exit

/**
* @brief puts a char on the console
* @param c char to be written
* @return void
*/
static void putchar(char c);

/**
* @brief puts a string on the console
* @param s string to be written
* @return void
*/
static void puts(const char * s);

/**
* @brief puts a formatted string on the console
* @param fmt formatted string
* @return void
*/
static void printf(const char * fmt, ...);

/**
* @brief gets a string from the console
* @param buf buffer to write string to
* @param n size of buffer (bytes)
* @return written buf
*/
static char * getsn(char * buf, size_t n);

/**
* @brief trek entry point
* @param io terminal io
* @return void
*/
extern void trek_start(struct io *io);

/**
* @brief sets seed for pseudo rng
* @return void
*/
static void rand_init(void);

/**
* @brief reads a char from console
* @return read char
*/
static char getchar(void);

static unsigned long rndst = 0;


// COMPILE-TIME CONFIGURATION
//

// defined(AEE0)
//  - Provides a trek_start(void) entry point
//  - Calls kputc() and kgetc() for terminal I/O
//
// defined(AEE2)
//  - Provides a trek_start(struct io * io) entry point
//  - calls ioread() and iowrite() for terminal I/O
//
// defined(AEE31)
//  - Provides a trek_start(struct io * io) entry point
//  - calls ioread() and iowrite() for terminal I/O
//  - implements ioread() and iowrite() in glue.c
//  - cannot link to kernel functions
//
// defined(AEE32)
//  - Provides a main(int argc, const char * argv[]) entry point
//  - calls _read() and _write() for terminal I/O

#if defined AEE0 + defined AEE1 + defined AEE2 + defined AEE31 + defined AEE32 > 1
#error "Exactly one of AEE0, AEE1, AEE2, AEE31, or AEE32 must be defined"
#endif

/**
* @brief reads a char from console
* @details kgetc wrapper
* @return char read from console
*/
static char gluegetc(void);

/**
* @brief writes a char to console
* @details kputc wrapper
* @param c char to write
* @param aux auxilary variable (unused)
* @return void
*/
static void glueputc(char c, void *aux);

//
// AEE0
//

#if defined AEE0

extern void kputc(char c);
extern char kgetc(void);
extern void kputs(const char * str);
extern void kvprintf(const char * fmt, va_list ap);
extern char * kgetsn(char * buf, size_t n);

extern size_t strlen(const char * s);
extern void * memset(void * s, int c, size_t n);
extern int strncmp(const char * s1, const char * s2, size_t n);
extern char * strncpy(char *dst, const char *src, size_t n);

void trek_start(struct io *io) {

extern void trek_main(void);

#ifndef RAND_SEED
		asm ("rdtime\t%0" : "=r"(rndst));
#else
		rndst = RAND_SEED;
#endif
		trek_main();
}

char gluegetc(void) {
		return kgetc();
}

void glueputc(char c,  void * __attribute__ ((unused)) aux) {
		kputc(c);
}

void printf(const char * fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
}

#elif defined AEE2
extern size_t strlen(const char * s);
extern void * memset(void * s, int c, size_t n);
extern int strncmp(const char * s1, const char * s2, size_t n);
extern char * strncpy(char *dst, const char *src, size_t n);

static struct io *io_trek;

void trek_start(struct io *io) {
		io_trek = io;
		extern void trek_main(void);
#ifndef RAND_SEED
		asm ("rdtime\t%0" : "=r"(rndst));
#else
		rndst = RAND_SEED;
#endif
		trek_main();
}

char gluegetc(void) {
		char buf;
		ioread(io_trek, &buf, 1);
		return buf;
}

void glueputc(char c, void * __attribute__ ((unused)) aux) {
		iowrite(io_trek, &c, 1);
}

void printf(const char * fmt, ...) {
	extern size_t vgprintf (
		void (*putcfn)(char, void*), void * aux,
		const char * fmt, va_list ap);

	va_list ap;

	va_start(ap, fmt);
	vgprintf((void*)putchar, NULL, fmt, ap);
	va_end(ap);
}

#endif

#ifndef RAND_MAX
#define RAND_MAX 0x7fffffffUL
#endif

void rand_init(void) {
#ifndef RAND_SEED
		asm ("rdtime\t%0" : "=r"(rndst));
#else
		rndst = RAND_SEED;
#endif
}

int rand(void) {
	rndst *= 25214903917UL;
	rndst += 11;
	return (rndst >> 12) % (RAND_MAX + 1);
}

static int abs(int x) {
	return (x < 0) ? -x : x;
}

int atoi(const char *s) {
	int val = 0;
	int neg = 0;

	while (*s == ' ')
		s += 1;
	
	if (*s == '-')
		neg = -1;
	
	while ('0' <= *s && *s <= '9') {
		val = 10 * val + *s - '0';
	s += 1;
	}
	
	return neg ? -val : val;
}

char * strcpy(char * s0, const char * s1) {
	char * sout = s0;
	while (*s1 != '\0')
		*s0++ = *s1++;
	*s0 = '\0';
	return sout;
}

char * strcat(char * s0, const char * s1) {
	char * sout = s0;
	while (*s0 != '\0')
		s0++;
	while (*s1 != '\0')
		*s0++ = *s1++;
	*s0 = '\0';
	return sout;
}

char getchar(void) {
	static char cprev = '\0';
	char c;

	// Convert \r followed by any number of \n to just \n

	do {
		c = gluegetc();
	} while (c == '\n' && cprev == '\r');
	
	cprev = c;

	if (c == '\r')
		return '\n';
	else
		return c;
}

void putchar(char c) {
	// Convert \n to \r\n

	if (c == '\n')
		glueputc('\r', NULL);
	glueputc(c, NULL);
}

#ifdef AEE0
void puts(const char * s) {
		kputs(s);
}
#elif defined AEE2
void puts(const char * s) {
	static const char crln[2] = "\r\n"; 
	const char * start;
	long cnt;

	if (s == NULL)
		return;
	
	// Try to minimize the number of device_write calls by writing multiple
	// characters that are not \n at once.

	for (;;) {
		start = s;
		cnt = 0;

		while (*s != '\0' && *s != '\n') {
			s += 1;
			cnt++;
		}
		if (s != start)
			iowrite(io_trek, start, cnt);
		iowrite(io_trek, crln, 2);
		if (*s == '\0')
			break;
		s += 1;
	}
}
#endif

#if defined AEE0
char * getsn(char * buf, size_t n) {
		return kgetsn(buf, n);
}

#elif defined AEE2

char * getsn(char * buf, size_t n) {
	char * p = buf;
	char c;

	for (;;) {
		c = getchar();

		switch (c) {
		case '\r':
			break;		
		case '\n':
			putchar('\n');
			*p = '\0';
			return buf;
		case '\b':
		case '\177':
			if (p != buf) {
				putchar('\b');
				putchar(' ');
				putchar('\b');

				p -= 1;
				n += 1;
			}
			break;
		default:
			if (n > 1) {
				putchar(c);
				*p++ = c;
				n -= 1;
			} else
				putchar('\a'); // bell
			break;
		}
	}
}

#endif
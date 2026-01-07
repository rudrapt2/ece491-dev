// glue.c - Glue code for Super Star Trek
//

#define AEE2

#include <stdarg.h>

#include "glue.h"

#if defined AEE0 + defined AEE1 + defined AEE2 + defined AEE31 + defined AEE32 > 1
#error "Exactly one of AEE0, AEE1, AEE2, AEE31, or AEE32 must be defined"
#endif

#if defined AEE0 + defined AEE1 + defined AEE2 + defined AEE31 + defined AEE32 < 1
#error "Exactly one of AEE0, AEE1, AEE2, AEE31, or AEE32 must be defined"
#endif

#ifndef RAND_MAX
#define RAND_MAX 0x7fffffffUL
#endif

int exited = 0;

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

#ifdef AEE0
static char gluegetc(void);
static void glueputc(char c);
static char getchar(void);

static unsigned long rndst = 0;

extern void kputc(char c);
extern char kgetc(void);
extern void kputs(const char * str);
extern void kvprintf(const char * fmt, va_list ap);
extern char * kgetsn(char * buf, size_t n);

extern size_t strlen(const char * s);
extern void * memset(void * s, int c, size_t n);
extern int strncmp(const char * s1, const char * s2, size_t n);
extern char * strncpy(char *dst, const char *src, size_t n);

void trek_start(void) {
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

void glueputc(char c) {
	kputc(c);
}

void printf(const char * fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
}

void puts(const char * s) {
	kputs(s);
}

char * getsn(char * buf, size_t n) {
	return kgetsn(buf, n);
}

int rand(void) {
	rndst *= 25214903917UL;
	rndst += 11;
	return (rndst >> 12) % (RAND_MAX + 1);
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
		glueputc('\r');
	glueputc(c);
}
#endif

#ifdef AEE2
static char gluegetc(void);
static void glueputc(char c);
static char getchar(void);

struct serial; // external object
static struct serial * trek_term;

// from device.c
extern int serial_recv(struct serial * ser, void * buf, unsigned int bufsz);
extern int serial_send(struct serial * ser, const void * buf, unsigned int len);

// from see.s
extern void halt(void);

static unsigned long rndst = 0;

void trek_start(struct serial * term, unsigned long rngseed) {
	extern void trek_main(void);
	trek_term = term;
#ifndef RAND_SEED
	if (rngseed == 0)
		rndst = 0xECE391;
	else
		rndst = rngseed;
	// rndst = RAND_SEED;
#endif
	trek_main();
}

char gluegetc(void) {
	int len;
	char c;

	len = serial_recv(trek_term, &c, 1);

	if (len != 1)
		halt();

	return c;
}

void glueputc(char c) {
	int len;

	len = serial_send(trek_term, &c, 1);

	if (len != 1)
		halt();
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
		glueputc('\r');
	glueputc(c);
}

void printf(const char * fmt, ...) {
	extern size_t vgprintf (
		void (*putcfn)(char, void*), void * aux,
		const char * fmt, va_list ap);

	va_list ap;

	va_start(ap, fmt);
	vgprintf((void(*)(char,void*))putchar, NULL, fmt, ap);
	va_end(ap);
}

void puts(const char * s) {
	const char * start;

	// Try to minimize the number of device_write calls by writing multiple
	// characters that are not \n at once.

	if (s != NULL) {
		for (;;) {
			start = s;
			while (*s != '\0' && *s != '\n')
				s += 1;
			if (s != start)
				serial_send(trek_term, start, s - start);
			serial_send(trek_term, "\r\n", 2);
			if (*s == '\0')
				break;
			s += 1;
		}
	}
}

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

int rand(void) {
	rndst *= 25214903917UL;
	rndst += 11;
	return (rndst >> 12) % (RAND_MAX + 1);
}

#endif

#ifdef AEE31
#include "ioimpl.h"
#include "kern_string.h"

static char getchar(void);

static unsigned long rndst = 0;
static struct io *io_trek;

long ioread(struct io * io, void * buf, long bufsz) {
    // no assert :(
	if (io == NULL) {
		printf("Error: passed I/O pointer is NULL\n");
		return -1;
	}
	
	if (io->intf == NULL) {
		printf("Error: I/O interface of passed I/O pointer is NULL");
		return -1;
	}

    if (io->intf->read == NULL)
        return -1;
    
    if (bufsz < 0)
        return -1;
    
    return io->intf->read(io, buf, bufsz);
}

long iowrite(struct io * io, const void * buf, long len) {
	long bufpos = 0; // position in buffer for next write
    long n; // result of last write

	// no assert :(
	if (io == NULL) {
		printf("Error: passed I/O pointer is NULL\n");
		return -1;
	}
	
	if (io->intf == NULL) {
		printf("Error: I/O interface of passed I/O pointer is NULL");
		return -1;
	}
    
    if (io->intf->write == NULL)
        return -1;

    if (len < 0)
        return -1;
    
    do {
        n = io->intf->write(io, buf+bufpos, len-bufpos);

        if (n <= 0)
            return (n < 0) ? n : bufpos;

        bufpos += n;
    } while (bufpos < len);

    return bufpos;
}

void main(struct io *io) {
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

void glueputc(char c) {
	iowrite(io_trek, &c, 1);
}

void printf(const char * fmt, ...) {
	extern size_t vgprintf (
		void (*putcfn)(char, void*), void * aux,
		const char * fmt, va_list ap);

	va_list ap;

	va_start(ap, fmt);
	vgprintf((void *)putchar, NULL, fmt, ap);
	va_end(ap);
}

void puts(const char * s) {
	const char * start;

	// Try to minimize the number of device_write calls by writing multiple
	// characters that are not \n at once.

	if (s != NULL) {
		for (;;) {
			start = s;
			while (*s != '\0' && *s != '\n')
				s += 1;
			if (s != start)
				iowrite(io_trek, start, s - start);
			iowrite(io_trek, "\r\n", 2);
			if (*s == '\0')
				break;
			s += 1;
		}
	}
}

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

int rand(void) {
	rndst *= 25214903917UL;
	rndst += 11;
	return (rndst >> 12) % (RAND_MAX + 1);
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
		glueputc('\r');
	glueputc(c);
}
#endif

#ifdef AEE32

#include "user_string.h"

static unsigned long rndst = 0;

void main(void) {
	extern void trek_main(void);
#ifndef RAND_SEED
	rndst = 391;
	// asm ("rdtime\t%0" : "=r"(rndst));
#else
	rndst = RAND_SEED;
#endif
	trek_main();
}

int rand(void) {
	rndst *= 25214903917UL;
	rndst += 11;
	return (rndst >> 12) % (RAND_MAX + 1);
}

void putchar(char c) {
	putc(c);
}
#endif

// UNCHANGED ACROSS MP TARGETS
// 
int abs(int x) {
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

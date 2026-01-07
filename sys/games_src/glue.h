#ifndef _GLUE_H_
#define _GLUE_H_

#include <stddef.h> // size_t

extern int rand(void);
extern int abs(int x);
extern int atoi(const char *s);
extern char * strcpy(char * s0, const char * s1);
extern char * strcat(char * s0, const char * s1);
extern int exited; // 1 if user is trying to exit
extern void putchar(char c);
extern void puts(const char * s);
extern void printf(const char * fmt, ...);
extern char * getsn(char * buf, size_t n);

extern size_t strlen(const char * s);
extern void * memset(void * s, int c, size_t n);
extern int strncmp(const char * s1, const char * s2, size_t n);
extern char * strncpy(char *dst, const char *src, size_t n);

#endif

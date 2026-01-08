// glue.h
//

#ifndef _GLUE_H_
#define _GLUE_H_

// #include "io.h"
#include <stdint.h>
#include "string.h"
#include "syscall.h"

// extern struct io_intf * stdio;
extern const char * const termcap;

// extern int printf(const char * fmt, ...);
extern int sprintf(char * buf, const char * fmt, ... );
extern char * strcpy(char * dst, const char * src);
extern char * strcat(char * dst, const char * src);
extern int fopen(const char *fname);
extern int fclose(int fd);
extern void fputs(const char *str, int fd);
extern void fputc(int fd, char c);
extern int fwrite(const void *ptr, size_t size, size_t nmemb, int fd);
extern int fread(void *ptr, size_t size, size_t nmemb, int fd);
extern void sleep(unsigned int cnt);
extern int putchar(int c);
extern int getchar(void);
extern void exit(void);

extern int random_seed(void);

#endif // _GLUE_H_
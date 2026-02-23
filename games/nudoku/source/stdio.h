// stdio.h - Minimal stdio compatibility for nudoku on bare-metal RISC-V
//
// Provides FILE* I/O on top of the OS _open/_read/_write/_close syscalls.
// Only the subset used by nudoku's save-game code is implemented.
//

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>   // size_t, NULL
#include <stdarg.h>   // va_list

#define EOF  (-1)

// Minimal FILE backed by a raw file descriptor
typedef struct {
    int fd;
} FILE;

// Declare standard stdio functions; implemented in glue.c
extern FILE *fopen(const char *path, const char *mode);
extern int   fclose(FILE *fp);
extern int   fgetc(FILE *fp);
extern int   fprintf(FILE *fp, const char *fmt, ...);
extern char *fgets(char *buf, int n, FILE *fp);

#endif // _STDIO_H_

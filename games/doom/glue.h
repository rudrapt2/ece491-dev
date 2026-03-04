// glue.h
//

#ifndef _GLUE_H_
#define _GLUE_H_

#include <sys/types.h>
#include <usr/string.h>
#include <usr/syscall.h>
#include <usr/io.h>
#include <usr/heap.h>

#include <stddef.h>
#include <stdint.h>

#define SEEK_END 0
#define SEEK_SET 1

#define EISDIR -EACCESS

extern int errno;

typedef struct FILE {
    int fd;
} FILE;

extern FILE* stderr;
extern FILE* stdout;

extern FILE* fopen(const char *filename, const char *mode);
extern int fseek(FILE *stream, long offset, int whence);
extern int fclose(FILE *stream);
extern long ftell(FILE *stream);
extern int fprintf(FILE *stream, const char * fmt, ...);
extern int vfprintf(FILE *stream, const char *format, va_list arg);
extern size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
extern size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
extern int fflush(FILE *stream);

extern int remove(const char *filename);
extern int rename(const char *old_filename, const char *new_filename);

extern int putchar(int character);
extern int sscanf(const char *str, const char *format, int * result); // fake

extern void *realloc(void *ptr, size_t size);
extern int atoi(const char *str);
extern void __attribute__ ((noreturn)) exit(int code);
extern int abs(int x);

// unsupported functions
extern int system(const char* command);
extern int mkdir(const char *path, mode_t mode);
extern double fabs(double x);
double atof(const char *str);

#endif // _GLUE_H_

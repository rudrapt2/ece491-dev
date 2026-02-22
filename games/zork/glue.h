// glue.h
//

#ifndef _GLUE_H_
#define _GLUE_H_

#include "usr/string.h"
#include "usr/syscall.h"
#include "usr/io.h"

#include <stddef.h>
#include <stdint.h>

// defs to make NONBUSINESS work
#define NANOSECONDS_PER_SECOND  1000000000
#define SECONDS_PER_MINUTE  60
#define SECONDS_PER_HOUR    (60*SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY     (24*SECONDS_PER_HOUR)
#define SECONDS_PER_WEEK    (7*SECONDS_PER_DAY)
#define NONBUSINESS
#define EOF                 -1

#define RAND_DEFAULT    391

extern int rand(void);
extern int time(uint64_t *timebuf);
extern int fopen(const char *fname);
extern int ftell(int fd);
extern int fseek(int fd, uint64_t pos);
extern int fclose(int fd);
extern int fgetc(int fd);
extern int fwrite(const void *ptr, size_t size, size_t nmemb, int fd);
extern int fread(void *ptr, size_t size, size_t nmemb, int fd);
#ifdef AEE31
extern void printf(const char * fmt, ...);
#endif

extern void exit(int result);

#endif // _GLUE_H_

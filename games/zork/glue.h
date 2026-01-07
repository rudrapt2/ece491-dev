// glue.h
//

#ifndef _GLUE_H_
#define _GLUE_H_

// #include "heap.h"
// #include "see.h"
#include "string.h"

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

#define STARTING_FD     3
#define RAND_DEFAULT    391

extern int rand(void);
extern int time(uint64_t *timebuf);
extern void halt_success(void);
extern int fopen(const char *fname);
extern int devopen(const char *name, int instno);
extern int ftell(int fd);
extern int fseek(int fd, uint64_t pos);
extern int fclose(int fd);
extern int fgetc(int fd);
extern int fwrite(const void *ptr, size_t size, size_t nmemb, int fd);
extern int fread(void *ptr, size_t size, size_t nmemb, int fd);
// extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE)

extern void exit(int result);

#define islower(c) ('a' <= (c) && (c) <= 'z')
#define toupper(c) ((c) - 'a' + 'A')


#endif // _GLUE_H_
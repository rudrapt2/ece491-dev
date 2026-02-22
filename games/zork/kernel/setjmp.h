#ifndef SETJMP_H
#define SETJMP_H

// jmp_buf stores: ra, sp, s0-s11  (14 x 8 bytes = 112 bytes)
typedef unsigned long jmp_buf[14];

int setjmp(jmp_buf buf);
void __attribute__((noreturn)) longjmp(jmp_buf buf, int val);

#endif // SETJMP_H

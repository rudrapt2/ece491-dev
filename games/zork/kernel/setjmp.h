#ifndef SETJMP_H
#define SETJMP_H

typedef unsigned long jmp_buf[14];

int setjmp(jmp_buf buf);
void __attribute__((noreturn)) longjmp(jmp_buf buf, int val);

#endif // SETJMP_H

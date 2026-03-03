#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include <stdlib.h>

unsigned int fib(unsigned int i) {
    if (i <= 1)
        return i;
    return fib(i-1) + fib(i-2);
}

void main (int argc, char** argv)
{
    unsigned  int n;
    if (argc != 2) {
        printf("USAGE: %s [FIB_NUM]\n", argv[0]);
        return;
    }

    n = strtoul(argv[1], NULL, 10);
    for (int i=1; i<n; i++) {
        dprintf(STDOUT, "%d\n", fib(i));
    }
}
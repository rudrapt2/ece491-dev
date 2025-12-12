#include "../syscall.h"
#include "../string.h"
#include "../shell.h"

void main (int argc, char* argv[])
{
    int i;
    
    if (argc < 3) {
        printf("Usage: append [append string] [string1] [string2] ...\n");
        return;
    }

    for (i=2; i<argc; i++) {
        dprintf(STDOUT, "%s%s\n", argv[1], argv[i]);
    }
}
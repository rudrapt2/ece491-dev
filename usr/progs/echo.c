#include "../syscall.h"
#include "../string.h"
#include "../shell.h"

void main (int argc, char** argv)
{
    int i;

    if (argc < 2) {
        printf("Usage: echo [string1] [string2] ...\n");
        return;
    }
    
    for (i=1; i<argc; i++) {
        dprintf(STDOUT, "%s ", argv[i]);
    }
    dprintf(STDOUT, "\n");
}
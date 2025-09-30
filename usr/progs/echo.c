#include "syscall.h"
#include "string.h"

int main (int argc, char** argv)
{
    int i;
    
    for (i=1; i<argc; i++) {
        dprintf(1, "%s ", argv[i]);
    }
    dprintf(1, "\n");
}
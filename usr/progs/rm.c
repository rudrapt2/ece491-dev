#include "../syscall.h"
#include "../string.h"
#include "../error.h"
#include "../shell.h"

void main (int argc, char** argv)
{
    int result;
    
    if (argc < 2) {
        printf("Usage: rm [file name] [file name] ...\n");
        return;
    }
    
    while (--argc) {
        argv++;
        result = _delete(argv[0]);

        if (result < 0) {
            printf("Could not delete file %s: %s\n", argv[0], error_name(result));
            continue;
        }
        dprintf(STDOUT, "Successfully removed %s\n", argv[0]);
    }
}
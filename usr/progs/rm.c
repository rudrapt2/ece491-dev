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
    
    for (int i = 1; i < argc; i++) {
        result = _delete(argv[i]);

        if (result < 0) {
            printf("%s: failed to delete %s (%s)\n", 
                argv[0], argv[i], error_desc(result));
            continue;
        }
    }
}
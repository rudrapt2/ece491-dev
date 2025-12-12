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
        result = _fsdelete(argv[0]);

        if (result < 0) {
            printf("Failed to delete file %s: ", argv[0]);
            switch (result) {
                case -ENOENT:
                    printf("No such file or directory\n");
                    break;
                default:
                    printf("Failed to remove file\n");
                    break;
            }
            continue;
        }
        dprintf(STDOUT, "Successfully removed %s\n", argv[0]);
    }
}
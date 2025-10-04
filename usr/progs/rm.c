#include "syscall.h"
#include "string.h"
#include "error.h"

void main (int argc, char** argv)
{
    int result;
    
    if (argc != 2) {
        printf("Usage: rm [file name]\n");
        return;
    }
    
    result = _fsdelete(argv[1]);

    if (result < 0) {
        printf("Failed to delete file %s: ", argv[1]);
        switch (result) {
            case -ENOENT:
                printf("No such file or directory\n");
                break;
            default:
                printf("Failed to remove file");
                break;
        }
        return;
    }
    
    dprintf(1, "Successfully removed %s\n", argv[1]);
    return;
}
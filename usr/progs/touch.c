#include "syscall.h"
#include "string.h"
#include "error.h"
#include "shell.h"

void main (int argc, char** argv)
{
    int result;
    
    if (argc != 2) {
        printf("Usage: touch [file name]\n");
        return;
    }
    
    result = _fscreate(argv[1]);

    if (result < 0) {
        printf("Failed to create file %s: ", argv[1]);
        switch (result) {
            case -EINVAL:
                printf("File Name Invalid\n");
                break;
            case -EMFILE:
                printf("File Already Exists\n");
                break;
            case -ENOINODEBLKS:
                printf("No Inode Blocks\n");
                break;
            default:
                printf("Failed to Create File\n");
                break;
        }
        return;
    }
    
    dprintf(STDOUT, "Successfully created %s\n", argv[1]);
    return;
}
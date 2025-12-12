#include "../syscall.h"
#include "../string.h"
#include "../error.h"
#include "../shell.h"

void main (int argc, char** argv)
{
    int result;
    
    if (argc < 2) {
        printf("Usage: touch [file name] [file name] ...\n");
        return;
    }

    while (--argc) {
        argv++;
    
        result = _fscreate(argv[0]);

        if (result < 0) {
            printf("Failed to create file %s: ", argv[0]);
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
            continue;
        }
        dprintf(STDOUT, "Successfully created %s\n", argv[0]);
    }
}
#include "../syscall.h"
#include "../string.h"
#include "../error.h"
#include "../shell.h"

void main (int argc, char** argv)
{
    unsigned long num_files;
    char buffer[14];
    
    if (argc != 2) {
        printf("Usage: open_many [num files]\n");
        return;
    }
    num_files = strtoul(argv[1], NULL, 10);

    // now open all the files to test limits
    for (int i = 0; i < num_files ; i++) {
        snprintf(buffer, sizeof(buffer), "c/file_%d", i);
        int fd = _open(-1, buffer);
        if (fd < 0) {
            printf("Failed to open file %s: ", buffer);
            switch (fd) {
                case -ENOENT:
                    printf("File Does Not Exist\n");
                    break;
                case -EBUSY:
                    printf("File Already Opened\n");
                    break;
                default:
                    printf("Failed to Open File\n");
                    break;
            }
            continue;
        }
        dprintf(STDOUT, "Successfully opened %s with fd %d\n", buffer, fd);
        _close(fd);
    }
}
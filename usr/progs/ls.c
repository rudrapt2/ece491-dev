#include "../syscall.h"
#include "../string.h"
#include "../shell.h"

#define BUFSZ 512

void main (int argc, char** argv)
{
    int fd, result;
    char buffer[BUFSZ+1];
    int newline = 0;
    buffer[BUFSZ] = '\0';
    
    if (argc > 1)
        fd = _open(-1, argv[1]);
    else 
        fd = _open(-1, "");
        
    if (fd < 0) {
        printf("%s: Directory Not Found\n", argv[1]);
        return;
    }

    while (1) {
        result = _read(fd, buffer, BUFSZ);
        if (result < 0) {
            printf("Read failed!\n");
            return;
        }

        if (result == 0) break;
        
        if (newline) dprintf(STDOUT, "\n");
        dprintf(STDOUT, "%s", buffer);
        newline = 1;
    }
    
    dprintf(1,"\n");
}
#include "../syscall.h"
#include "../string.h"
#include "../shell.h"

#define BUFSZ 512

void main (int argc, char** argv)
{
    int fd, result;
    char buffer[BUFSZ+1];
    buffer[BUFSZ] = '\0';
    
    if (argc != 2) {
        printf("Usage: cat [file]\n");
        return;
    }
    
    fd = _open(-1, argv[1]);
    if (fd < 0) {
        printf("%s: File Not Found\n", argv[1]);
        return;
    }
    
    while (1) {
        result = _read(fd, buffer, BUFSZ);
        if (result < 0) {
            printf("Read failed!\n");
            return;
        }
        if (result == 0) {
            printf("\r\n");
            return;
        }
        buffer[result] = '\0'; 
        dprintf(STDOUT, buffer);
    }
}
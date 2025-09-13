#include "syscall.h"
#include "string.h"

#define BUFSZ 512

int main (int argc, char** argv)
{
    int fd, result;
    char buffer[BUFSZ+1];
    buffer[BUFSZ] = '\0';
    
    if (argc != 2) {
        printf("Usage: cat [file]\n");
        _exit();
    }
    
    fd = _fsopen(-1, argv[1]);
    if (fd < 0) {
        printf("%s: File Not Found\n", argv[1]);
        _exit();
    }
    
    while (1) {
        result = _read(fd, buffer, BUFSZ);
        if (result < 0) {
            printf("Read failed!\n");
            _exit();
        }
        if (result == 0) {
            dprintf(1, "\r\n");
            _exit();
        }
        dprintf(1, buffer);
    }
}
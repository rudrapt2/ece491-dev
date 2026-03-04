#ifdef STUDENT
    // YOUR CODE HERE
#else

#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../error.h"

#define BUFSZ 512

void main (int argc, char** argv)
{
    int fd, result;
    char buffer[BUFSZ+1];
    buffer[BUFSZ] = '\0';
    char* dir = "";
    
    if (argc > 1)
        dir = argv[1];

    fd = _open(-1, dir);
        
    if (fd < 0) {
        printf("%s: could not open directory %s (%s)\n", 
            argv[0], dir, error_desc(fd));
        return;
    }

    for (;;) {
        result = _read(fd, buffer, BUFSZ);
        if (result < 0) {
            printf("%s: failed to read \"%s\" (%s)\n", 
                argv[0], dir, error_desc(result));
            return;
        }

        if (result == 0) break; // EOF
        dprintf(STDOUT, "%s\n", buffer);
    }
}

#endif
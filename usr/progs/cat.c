#ifdef STUDENT
    // YOUR CODE HERE
#else

#include "../syscall.h"
#include "../shell.h"
#include "../error.h"

#define BUFSZ 512

static int cat_stream(int fd) {
    char buffer[BUFSZ];
    int result;

    for (;;) {
        result = _read(fd, buffer, BUFSZ);
        if (result <= 0) 
            return result;
        if (buffer[result] == 3 && fd == STDIN) {
            printf("Killed\n");
            return 0;
        }
        _write(STDOUT, buffer, result);
    }
}

void main(int argc, char** argv)
{
    int result;
    // no args, read from STDIN
    if (argc == 1) {
        result = cat_stream(STDIN);
        if (result)
            printf("%s: failed to read from STDIN (%s)\n", 
                argv[0], error_desc(result));
        return;
    }

    // one or more files
    for (int i = 1; i < argc; i++) {
        int fd = _open(-1, argv[i]);
        if (fd < 0) {
            printf("%s: could not open %s (%s)\n", 
                argv[0], argv[i], error_desc(fd));
            continue;
        }
        result = cat_stream(fd);
        if (result)
            printf("%s: failed to read %s (%s)\n", 
                argv[0], argv[i], error_desc(result));
        _close(fd);
    }
}

#endif
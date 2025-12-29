#include "../syscall.h"
#include "../shell.h"
#include "../error.h"

#define BUFSZ 512

static void cat_stream(int fd) {
    char buffer[BUFSZ];
    int result;

    while (1) {
        result = _read(fd, buffer, BUFSZ);
        if (result < 0) {
            printf("Read failed!\n");
            return;
        }
        if (result == 0) {
            return; // EOF
        }
        _write(STDOUT, buffer, result);
    }
}

void main(int argc, char** argv)
{
    // no args, read from STDIN
    if (argc == 1) {
        cat_stream(STDIN);
        return;
    }

    // one or more files
    for (int i = 1; i < argc; i++) {
        int fd = _open(-1, argv[i]);
        if (fd < 0) {
            printf("Could not open %s: %s\n", argv[i], error_name(fd));
            continue;
        }
        cat_stream(fd);
        _close(fd);
    }
}
#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../heap.h"

#define BUFSZ 512

void exec(int c, char** v) {
	char path[256];
	int fd, result;

	// Null-terminate the argument array
	v[c] = NULL;

	// If path doesn't start with '/', prepend '/c/' for relative paths
	if (strncmp(v[0], "/", 1) != 0 && strncmp(v[0], "c/", 2) != 0) {
		snprintf(path, sizeof(path), "/c/%s", v[0]);
	}
	else {
		strncpy(path, v[0], sizeof(path) - 1);
	}

	// Open the executable file
	fd = _open(-1, path);

	if (fd < 0) {
		printf("Unable to access %s (Error Code: %d)\n", path, fd);
		_exit();
	}

	result = _exec(fd, c, v);
	printf("Failed to exec file (Error Code: %d)", result);
	_exit();
}

void main (int argc, char** argv)
{
    int maxc = argc - 1;
    int c = argc - 1;
    char** v = argv+1;
    char** tempv;
    char* buf;
    char read;
    int num_read;
    int result;

    if (argc < 2) {
        printf("Usage: xargs [cmd] [...] < argfile\n");
        return;
    }

    // parse input lines
    for (;;) {
        while (c+1 >= maxc) {
            tempv = malloc((maxc * 2) * sizeof(char*));
            memcpy(tempv, v, c * sizeof(char*));
            v = tempv;
            maxc *= 2;
        }

        buf = malloc(BUFSZ);
        num_read = 0;

        // read a line from stdin
        for (;;) {
            result = _read(STDIN, &read, 1);

            if (result < 0) {
                printf("Read failed!\n");
                return;
            }

            if (result == 0 || read == '\r' || read == ' ') {
                buf[num_read] = '\0';
                break;
            }

            if (read == '\n') continue;

            if (read == (char)3) {
                printf("Program Killed\n");
                return;
            }

            
            buf[num_read++] = read;
            
            if (num_read == BUFSZ - 1) {
                printf("Input line %d too long\n", c - argc + 2);
                return;
            }
        }

        // skip empty lines
        if (buf[0] != '\0' &&
            buf[0] != ' '  &&
            buf[0] != '\n' &&
            buf[0] != '\r')
            v[c++] = buf;

        if (result == 0) break;
    }

    v[c] = NULL;
    exec(c, v);
}
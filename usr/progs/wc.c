#ifdef STUDENT
    // YOUR CODE HERE
#else

#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../error.h"

void main (int argc, char** argv)
{
    char buffer;
    int result;
    int in_word = 0;
    int nc = 0;
    int wc = 0;
    int cc = 0;

    int in_stream = STDIN;
    
    if (argc != 1) {
        in_stream = _open(-1, argv[1]);
        if (in_stream < 0) {
            printf("%s: failed to open %s (%s)\n", 
                argv[0], argv[1], error_desc(in_stream));
            return;
        }
    }
    
    while (1) {
        result = _read(in_stream, &buffer, 1);
        if (result < 0) {
            printf("%s: failed to read (%s)\n", 
                argv[0], error_desc(result));
            return;
        }
        if (buffer == 3 && in_stream == STDIN) {
            printf("Killed\n");
            return;
        }
        if (result == 0) {
            dprintf(STDOUT, "\t%d\t%d\t%d\n", nc, wc, cc);
            return;
        }
        cc++;
        switch (buffer) {
            case '\n':
                nc++;
            case '\r':
            case '\t':
            case ' ':
                in_word = 0;
                break;
            default:
                if (!in_word) wc++;
                in_word = 1;
                break;
        }
    }
}

#endif
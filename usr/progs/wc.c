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
            printf("Could not open file %s: %s\n", argv[1], error_name(in_stream));
            return;
        }
    }
    
    while (1) {
        result = _read(in_stream, &buffer, 1);
        if (result < 0) {
            printf("Could not read buffer: %s\n", error_name(result));
            return;
        }
        if (buffer == 3 && in_stream == STDIN) {
            printf("Program Killed\n");
            return;
        }
        if (result == 0) {
            dprintf(STDOUT, "\t%d\t%d\t%d\n", nc, wc, cc);
            return;
        }
        cc++;
        nc += (buffer=='\n');
        if (buffer==' ' || buffer=='\n') in_word = 0;
        else {
            if(in_word==0) wc++;
            in_word = 1;
        }
    }
}
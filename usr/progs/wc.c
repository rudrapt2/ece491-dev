#include "syscall.h"
#include "string.h"
#include "shell.h"

int main (int argc, char** argv)
{
    char buffer;
    int result;
    int in_word = 0;
    int nc = 0;
    int wc = 0;
    int cc = 0;
    
    if (argc != 1) {
        printf("Usage: wc < file\n");
        _exit();
    }
    
    while (1) {
        result = _read(STDIN, &buffer, 1);
        if (result < 0) {
            printf("Read failed!\n");
            _exit();
        }
        if (buffer == 3) {
            printf("Program Killed\n");
            _exit();
        }
        if (result == 0) {
            dprintf(STDOUT, "\t%d\t%d\t%d\n", nc, wc, cc);
            _exit();
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
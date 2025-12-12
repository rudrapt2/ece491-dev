// NOTE: THIS DOES NOT WORK LOL
#include "../syscall.h"
#include "../string.h"
#include "../shell.h"

void main (int argc, char** argv)
{
    int i;

    if (argc < 2) {
        printf("Usage: printf [format] [arg1] [arg2] ...\n");
        return;
    }

    switch (argc) {
        case 2: dprintf(1, argv[1]); break;
        case 3: dprintf(1, argv[1], argv[2]); break;
        case 4: dprintf(1, argv[1], argv[2], argv[3]); break;
        case 5: dprintf(1, argv[1], argv[2], argv[3], argv[4]); break;
        case 6: dprintf(1, argv[1], argv[2], argv[3], argv[4], argv[5]); break;
        case 7: dprintf(1, argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]); break;
        case 8: dprintf(1, argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]); break;
        default:
            dprintf(1, argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
            for (i = 8; i < argc; ++i) {
                printf(" %s", argv[i]);
            }
            break;
    }

    printf("\n");
}
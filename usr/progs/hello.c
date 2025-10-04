#ifdef UMODE
    #include "string.h"
    #include "syscall.h"
#endif

void main(void) {
    #ifdef UMODE
        _print("Hello, world!\n");
        _exit();
    #endif
}
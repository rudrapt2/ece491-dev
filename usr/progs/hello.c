#ifdef UMODE
    #include "string.h"
    #include "syscall.h"
#endif

void main(void) {
    #ifdef UMODE
        dprintf(1, "Hello, world!\n");
        _exit();
    #endif
}
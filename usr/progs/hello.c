#ifndef UMODE // cp1
    #include "../io.h"
    void main(struct io * termio) {
        // outputs to termio
        ioprintf(termio, "Hello, world!\n");
    }
#else  // cp2&3
    #include "string.h"
    void main(void) {
        // outputs to io in fd2
        printf("Hello, world!\n");
    }
#endif
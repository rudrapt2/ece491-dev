// glue.h - Declarations for nudoku bare-metal glue (AEE32 / MP3 CP2)
//

#ifndef _GLUE_H_
#define _GLUE_H_

#include <stddef.h>

// atexit - register function to call at exit()
extern int atexit(void (*fn)(void));

#endif // _GLUE_H_

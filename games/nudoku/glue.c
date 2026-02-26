// glue.c - Runtime/syscall wrapper for nudoku (AEE32 / MP3 CP2)
//
// Provides POSIX-like runtime functions not available in the bare-metal
// environment. Keep this file focused on process/runtime adaptation,
// syscalls, and compatibility shims.
// ncurses implementation details live in curses.c.
//

#ifdef AEE32

#include "usr/string.h"     // strlen, snprintf, vsnprintf, printf, memcpy, ...
#include "usr/heap.h"       // malloc, free
#include "usr/syscall.h"    // _exit, _open, _close, _read, _write, _delete, _usleep
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// ── Internal RTC fd ───────────────────────────────────────────────────────────

int rtc_fd = -1;

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    extern int nudoku_main(int argc, char *argv[]);

    // Open RTC for time()/srand seeding
    rtc_fd = _open(-1, "/dev/rtc");

    // Seed RNG from hardware entropy; fall back to RTC or constant
    {
        unsigned long long seed = 0;
        int rng = _open(-1, "/dev/viorng0");
        if (rng >= 0) {
            _read(rng, &seed, sizeof(seed));
            _close(rng);
        } else if (rtc_fd >= 0) {
            _read(rtc_fd, &seed, sizeof(seed));
        } else {
            seed = 391;
        }
        // Store in rand state (srand will be called by nudoku, so just prime it)
        extern void srand(unsigned int);
        srand((unsigned int)(seed ^ (seed >> 32)));
    }

    return nudoku_main(argc, argv);
}

// ── exit / abort ──────────────────────────────────────────────────────────────

// atexit handler table
#define ATEXIT_MAX 8
static void (*atexit_fns[ATEXIT_MAX])(void);
static int   atexit_cnt = 0;

int atexit(void (*fn)(void)) {
    if (atexit_cnt >= ATEXIT_MAX) return -1;
    atexit_fns[atexit_cnt++] = fn;
    return 0;
}

void exit(int status) {
    (void)status;
    // Run registered atexit handlers in LIFO order
    for (int i = atexit_cnt - 1; i >= 0; i--)
        atexit_fns[i]();
    _exit();
}

void abort(void) { _exit(); }

#endif // AEE32

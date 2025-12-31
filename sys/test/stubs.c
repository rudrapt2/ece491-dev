// stubs.c - Empty functions to allow test cases to link
//

#include <stdint.h>
#include "string.h"
#include "misc.h"

static void __attribute__ ((noreturn))
    stub_called_panic(const char * func_name);

#define DEFINE_STUB(func) \
__attribute__ ((weak)) void func(void) { \
    stub_called_panic(__func__); \
}

DEFINE_STUB(handle_smode_interrupt);
DEFINE_STUB(handle_umode_interrupt);
DEFINE_STUB(switch_mspace);
DEFINE_STUB(process_exit);
DEFINE_STUB(handle_umode_page_fault);
DEFINE_STUB(handle_syscall);
DEFINE_STUB(register_device);
DEFINE_STUB(enable_intr_source);
DEFINE_STUB(disable_intr_source);

void stub_called_panic(const char * func_name) {
    static char msgbuf[256];
    snprintf(msgbuf, sizeof(msgbuf), "%s called", func_name);
    panic(msgbuf);
}
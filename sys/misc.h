// misc.h - Miscellaneous functions and macros
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _MISC_H_
#define _MISC_H_

// The ROUND_UP and ROUND_DOWN macros round /n/ to a multiple of /k/. Argument
// /k/ is evaluated multiple times.

#define ROUND_UP(n,k) (((n)+(k)-1)/(k)*(k))
#define ROUND_DOWN(n,k) ((n)/(k)*(k))

// The ISPOW2 macro evaluates to 1 if its argument is either zero or a power of
// two. The argument must be an integer type. Cast pointers to `uintptr_t` to
// test pointer alignment. The argument is evaluated multiple times.

#define ISPOW2(n) (((n)&((n)-1)) == 0)


#endif // _MISC_H_
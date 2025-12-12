/*! @file shell.h
    @brief User shell constants
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA
*/
#include "string.h"
#include "syscall.h"

// shell terminators
#define FIN '<'
#define FOUT '>'
#define PIPE '|'

// standard file descriptors
#define STDIN 0
#define STDOUT 1
#define CONSOLEOUT 2

extern void exec(int c, char** v);
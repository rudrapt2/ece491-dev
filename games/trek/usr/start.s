# start.s - User application startup
#
# Copyright (c) 2024-2025 University of Illinois
# SPDX-License-identifier: NCSA
#

        .text

        .global _start
        .type   _start, @function
        
_start:
        
        la      ra, _exit
        j       main
        .end

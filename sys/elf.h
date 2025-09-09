/*! @file elf.h
    @brief ELF file loader    
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#ifndef _ELF_H_
#define _ELF_H_

#include "uio.h"

/*!
 * @brief Loads in an elf file given its io pointer, return the start of the entry point through eptr.
 * @details Ingests an io to read the executable and do some validation on it (to make sure it’s actually an executable). 
 * It is then the job of elf load to read the ELF header and process the program headers. 
 * The loader should only load program header entries of type PT LOAD. You must create an elf.c file that contains elf load, 
 * alongside any relevant structs and magic numbers (use #define!). You can find the layouts of structs and values of the magic numbers 
 * in the ELF Linux man page. You are allowed to look at .../uapi/linux/elf.h in the Linux source code. Notice that since elf load 
 * should support any compliant I/O interface, that we can in general load an ELF from “any source” as long as ioread and ioseek 
 * are implemented in the given io.
 * @param elfio Pointer to the io object that contains the elf file.
 * @param eptr Return pointer to the entry point.
 * @return Error code.
 */
extern int elf_load(struct uio * file, void (**eptr)(void));

#endif // _ELF_H_

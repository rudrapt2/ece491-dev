// error.h - Error numbers
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _ERROR_H_
#define _ERROR_H_

#define EINVAL      1   ///< Invalid argument
#define EBUSY       2   ///< Device or resource busy
#define ENOTSUP     3   ///< Operation not supported
#define ENODEV      4   ///< No such device
#define EIO         5   ///< I/O error
#define EBADFMT     6   ///< Bad format
#define ENOENT      7   ///< No such file or directory
#define EACCESS     8   ///< Permission denied
#define EBADFD      9   ///< File descriptor in bad state
#define EMFILE     10   ///< Too many open files
#define EMPROC     11   ///< Too many processes
#define EMTHR      12   ///< Too many threads
#define ECHILD     13   ///< No child process
#define ENOMEM     14   ///< Out of memory
#define EPIPE      15   ///< Broken pipe


// Returns a string with the error name (e.g. 2 => "EBUSY")

extern const char * error_name(int code);

#endif // _ERROR_H_
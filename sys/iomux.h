// iomux.h - I/O Multiplexing
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

extern int create_iomux4(struct io * cio, struct io * ch[4]);
extern int attach_iomux4(const char * cdev_name);


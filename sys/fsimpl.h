// fsimpl.h - File system implementers' interface
// 

#ifndef _FSIMPL_H_
#define _FSIMPL_H_

struct uio;

struct filesystem {
    int (*open)(struct filesystem * fs, const char * name, struct uio ** uioptr);
    int (*create)(struct filesystem * fs, const char * name);
    int (*delete)(struct filesystem * fs, const char * name);
    void (*flush)(struct filesystem * fs);
};

extern int attach_filesystem(const char * name, struct filesystem * fs);

#endif // _FSIMPL_H_
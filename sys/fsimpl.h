// fsimpl.h - File system implementers' interface
// 

struct filesystem {
    int (*open)(struct filesystem * fs, const char * name, struct uio ** uioptr);
    int (*creat)(struct filesystem * fs, const char * name);
    int (*unlink)(struct filesystem * fs, const char * name);
};

extern int attach_fs(const char * name, struct filesystem * fs);

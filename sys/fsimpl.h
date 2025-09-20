// fsimpl.h - File system implementers' interface
// 

struct filesystem {
    int (*open)(struct filesystem * fs, const char * name, struct uio ** uioptr);
    int (*create)(struct filesystem * fs, const char * name);
    int (*delete)(struct filesystem * fs, const char * name);
    void (*flush)(struct filesystem * fs);
};

extern int mount_fs(const char * name, struct filesystem * fs);

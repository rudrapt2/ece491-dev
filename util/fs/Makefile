CC = gcc

CFLAGS = -Wall -Werror=implicit-function-declaration
LDFLAGS = -lm

all: mkfs_ktfs unmkfs_ktfs

mkfs_ktfs: mkfs_ktfs.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

unmkfs_ktfs: unmkfs_ktfs.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mkfs_lffs: mkfs_lffs.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf *.o *.elf *.asm mkfs_ktfs unmkfs_ktfs mkfs_lffs

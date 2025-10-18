make -C ../../usr clean
make -C ../../usr
make
./mkfs_ktfs ../../sys/blob.raw 512K 16 ../../usr/bin/* # smaller so it fits in blob
./mkfs_ktfs ../../sys/ktfs.raw 35M 32 rudra_garbo small ../../usr/bin/* ../../usr/games/*

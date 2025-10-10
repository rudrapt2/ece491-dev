make -C ../../usr
make
./mkfs_ktfs ../../sys/ktfs.raw 35M 32 ../../usr/bin/* ../../usr/games/*

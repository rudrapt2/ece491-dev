make -C ../../usr
make
./mkfs_ktfs ../../sys/ktfs.raw 35M 32 rudra_garbo small ../../usr/bin/* ../../usr/games/*

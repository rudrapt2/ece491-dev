make -C ../../usr clean
make -C ../../usr && \
make && \
./mkfs_ktfs -R ../../sys/fs/ktfs.raw 32M 32 rudra_garbo small ../../usr/bin/* ../../usr/games/*

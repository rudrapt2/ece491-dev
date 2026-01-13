make -C ../../usr clean
make -C ../../usr && \
make && \
./mkfs_lffs -R ../../sys/lffs.raw 32M rudra_garbo small ../../usr/bin/* ../../usr/games/*

make -C ../../usr clean
make -C ../../usr && \
make && \
./mkfs_ngfs -R ../../sys/fs/ngfs.raw 32M rudra_garbo small ../../usr/bin/* ../../usr/games/*

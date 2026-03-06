make -C ../../usr clean
make -C ../../usr && \
make -C ../../games clean && \
make -C ../../games mp3-cp3 && \
make && \
./mkfs_ngfs ../../sys/fs/ngfs.raw -R 32M rudra_garbo small ../../usr/bin/* ../../usr/games/*

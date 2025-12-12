make -C ../../usr clean
make -C ../../usr && \
make && \
./mkfs_ktfs ../../sys/blob.raw 1M 16 ../../usr/bin/* 

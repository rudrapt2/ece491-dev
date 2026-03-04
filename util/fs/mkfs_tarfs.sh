make -C ../../usr clean
make -C ../../usr && \
make -C ../../games clean && \
make -C ../../games mp3-cp3 && \
tar -cf ../../sys/fs/tarfs.tar rudra_garbo small ../../usr/bin/* ../../usr/games/*

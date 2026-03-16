CC = riscv64-linux-gcc
CFLAGS = -Wall -Wextra -fno-pie 
LDFLAGS = -static -no-pie 

bin/%: progs/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) 

all: cp3

KERN_GAMES_FOLDER 	= ../sys/games
USR_GAMES_FOLDER 	= ../usr/games

mp2:
	mkdir -p $(KERN_GAMES_FOLDER)
	make -C trek trek-mp2
	make -C rule30 rule30-mp2

	cp trek/bin/* $(KERN_GAMES_FOLDER)
	cp rule30/bin/* $(KERN_GAMES_FOLDER)



mp3-cp2:
	mkdir -p $(USR_GAMES_FOLDER)
	
	make -C trek trek-mp3-cp2
	cp trek/bin/trek-mp3-cp2 $(USR_GAMES_FOLDER)/trek

	make -C rule30 rule30-mp3-cp2
	cp rule30/bin/rule30-mp3-cp2 $(USR_GAMES_FOLDER)/rule30

	make -C zork zork-mp3cp2.elf
	cp zork/dtextc.dat $(USR_GAMES_FOLDER)/dtextc.dat
	cp zork/zork-mp3cp2.elf $(USR_GAMES_FOLDER)/zork

	make -C ../util/fs all
	./../util/fs/mkfs_ngfs -R ../sys/fs/ngfs.raw 32M ../usr/bin/* ../usr/games/*

clean:
	rm -rf $(KERN_GAMES_FOLDER)
	rm -rf $(USR_GAMES_FOLDER)
	make -C trek clean
	make -C rule30 clean
	make -C zork clean
	make -C rogue clean
	make -C doom clean
	make -C flying-toasters clean
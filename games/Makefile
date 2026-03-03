all: cp3

KERN_GAMES_FOLDER 	= ../sys/games
USR_GAMES_FOLDER 	= ../usr/games

mp2:
	mkdir -p $(KERN_GAMES_FOLDER)
	
	make -C trek trek-mp2
	cp trek/bin/* $(KERN_GAMES_FOLDER)

	make -C rule30 rule30-mp2
	cp rule30/bin/* $(KERN_GAMES_FOLDER)

mp3-cp1:
	mkdir -p $(USR_GAMES_FOLDER)
	
	make -C trek trek-mp3-cp1
	cp trek/bin/trek-mp3-cp1 		$(USR_GAMES_FOLDER)/trek-mp3-cp1

# 	make -C rule30 rule30-mp3-cp1
# 	cp rule30/bin/rule30-mp3-cp1 	$(USR_GAMES_FOLDER)/rule30

	make -C zork zork-mp3-cp1
	cp zork/data/dtextc.dat 		$(USR_GAMES_FOLDER)/dtextc.dat
	cp zork/bin/zork-mp3-cp1		$(USR_GAMES_FOLDER)/zork-cp1

	make -C rogue rogue-mp3-cp1
	cp rogue/bin/rogue-mp3-cp1		$(USR_GAMES_FOLDER)/rogue

# 	make -C ../util/fs all
# 	./../util/fs/mkfs_ngfs -R ../sys/fs/ngfs.raw 32M ../usr/bin/* ../usr/games/*

mp3-cp2:
	mkdir -p $(USR_GAMES_FOLDER)
	
	make -C trek trek-mp3-cp2
	cp trek/bin/trek-mp3-cp2 		$(USR_GAMES_FOLDER)/trek

	make -C rule30 rule30-mp3-cp2
	cp rule30/bin/rule30-mp3-cp2 	$(USR_GAMES_FOLDER)/rule30

	make -C zork zork-mp3-cp2
	cp zork/data/dtextc.dat 		$(USR_GAMES_FOLDER)/dtextc.dat
	cp zork/bin/zork-mp3-cp2		$(USR_GAMES_FOLDER)/zork

	make -C rogue rogue-mp3-cp2
	cp rogue/bin/rogue-mp3-cp2		$(USR_GAMES_FOLDER)/rogue

	make -C nudoku nudoku-mp3-cp2
	cp nudoku/bin/nudoku-mp3-cp2	$(USR_GAMES_FOLDER)/nudoku

	make -C tetris tetris-mp3-cp2
	cp tetris/bin/tetris-mp3-cp2	$(USR_GAMES_FOLDER)/tetris

	make -C doom all
	cp doom/bin/doom				$(USR_GAMES_FOLDER)/doom
	cp doom/bin/doom1.wad			$(USR_GAMES_FOLDER)/doom1.wad
	cp doom/bin/doomlauncher		$(USR_GAMES_FOLDER)/doomlauncher

# 	make -C ../util/fs all
	./../util/fs/mkfs_ngfs -R ../sys/fs/ngfs.raw 32M ../usr/bin/* ../usr/games/*

clean:
	rm -rf $(KERN_GAMES_FOLDER)
	rm -rf $(USR_GAMES_FOLDER)
	make -C trek clean
	make -C rule30 clean
	make -C zork clean
	make -C rogue clean
	make -C doom clean
	make -C nudoku clean
	make -C flying-toasters clean
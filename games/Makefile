all: cp3

KERN_GAMES_FOLDER = ../sys/games

mp2:
	mkdir -p $(KERN_GAMES_FOLDER)
	make -C trek trek-mp2
	make -C rule30 rule30-mp2

	cp trek/bin/* $(KERN_GAMES_FOLDER)
	cp rule30/bin/* $(KERN_GAMES_FOLDER)


USR_GAMES_FOLDER = ../usr/games
cp3:
# 	mkdir -p games 
# 	make -C trek trek_mp3_cp2.elf
# 	make -C zork
# 	make -C rogue 
# 	make -C doom
# 	make -C flying-toasters
# 	cp trek/*.elf games/trek
# 	cp zork/*.elf games/zork
# 	cp zork/*.dat games/
# 	cp rogue/*.elf games/rogue
# 	cp doom/bin/* games
# 	cp flying-toasters/games/* games
	mkdir -p $(USR_GAMES_FOLDER)
	make -C doom

	
	cp doom/bin/* $(USR_GAMES_FOLDER)

clean:
	rm -rf $(KERN_GAMES_FOLDER)
	rm -rf $(USR_GAMES_FOLDER)
	make -C trek clean
	make -C zork clean
	make -C rogue clean
	make -C doom clean
	make -C flying-toasters clean
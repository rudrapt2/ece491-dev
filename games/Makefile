all: cp3

cp3:
	mkdir -p games 
	make -C trek trek_mp3_cp2.elf
	make -C zork
	make -C rogue 
	make -C doom
	make -C flying-toasters
	cp trek/*.elf games/trek
	cp zork/*.elf games/zork
	cp zork/*.dat games/
	cp rogue/*.elf games/rogue
	cp doom/bin/* games
	cp flying-toasters/games/* games

clean:
	rm -f games/*
	make -C trek clean
	make -C zork clean
	make -C rogue clean
	make -C doom clean
	make -C flying-toasters clean
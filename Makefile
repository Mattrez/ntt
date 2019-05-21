all:
	$(CC) -o ntt networktrafic.c -Wall -Wextra -O3
debug:
	$(CC) -o debug_ntt networktrafic.c -g
install: all
	cp ntt /usr/bin/

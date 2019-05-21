all:
	$(CC) -o ntt networktrafic.c -Wall -Wextra -O3

debug:
	$(CC) -o debug_ntt networktrafic.c -g

install: all
	cp -f ntt /usr/bin/
	chmod 755 /usr/bin/ntt

uninstall:
	rm -f /usr/bin/ntt

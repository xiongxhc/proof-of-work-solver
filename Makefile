CC = gcc -std=gnu99

all: server

server: server.c uint256.h sha256.o
	$(CC) -g -o server server.c sha256.o -lpthread

sha256.o: sha256.h sha256.c
	$(CC) -g -c -o sha256.o sha256.c

clean:
	rm -rf *.o server

all: clean client server

client:
	gcc -Wall -o client client.c

server:
	gcc -Wall -o server server.c -lm

clean:
	rm -f client server *.o


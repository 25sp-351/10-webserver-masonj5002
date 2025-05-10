all: webserver

CC = gcc
CFLAGS = -Wall

webserver: webserver.o
	$(CC) $(CFLAGS) webserver.o -o webserver

webserver.o: webserver.c
	$(CC) $(CFLAGS) -c webserver.c

clean:
	rm -f *.o webserver
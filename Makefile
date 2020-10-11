# Makefile

CC = gcc
CFLAGS  = -Wall -g
OBJ = server.o
DEST = server

all: $(DEST)

build: $(OBJ)
	$(CC) $(CFLAGS) -o $(DEST) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(OBJ)
	rm -rf $(DEST)

install:
	cp -r $(DEST) /usr/local/bin
	cp -r systemd/server_iot.service /etc/systemd/system/

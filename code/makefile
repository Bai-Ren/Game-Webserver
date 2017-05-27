MAX_EVENTS = 16

OBJS = .objs/server.o .objs/io.o .objs/setupServer.o

CC = gcc
WARNINGS = -Wall -Wextra -Wno-unused -Wno-unused-parameter
CFLAGS = -O2 $(WARNINGS) -std=c99 -c  -D_GNU_SOURCE -D MAX_EVENTS=$(MAX_EVENTS)

LD = gcc
LDFLAGS = 

.PHONY: all
all: server

.objs/io.o: io.c io.h
	$(CC) $(CFLAGS) $< -o $@

.objs/setupServer.o: setupServer.c setupServer.h
	$(CC) $(CFLAGS) $< -o $@

.objs/server.o: server.c
	$(CC) $(CFLAGS) $< -o $@

server: $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean: 
	rm -f server $(OBJS)
CC=gcc

LIBS=-lrt

DEPS = cmn.h

CFLAGS = -Wall -Wextra -g -std=c99 -D_XOPEN_SOURCE

OBJ_CONS = cmn.o cons.o 
OBJ_PROD = cmn.o prod.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: producer consumer

producer: $(OBJ_PROD)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

consumer: $(OBJ_CONS)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm *.o

CC = gcc
CFLAGS = -std=gnu89 -g -Wall -Wextra -Wno-implicit-function-declaration -Wno-implicit-int -Wno-builtin-declaration-mismatch -fno-builtin -I. -I../pflayer

all: rmlayer.o

rm.o: rm.c rm.h
	$(CC) $(CFLAGS) -c rm.c -o rm.o

rmlayer.o: rm.o
	ld -r -o rmlayer.o rm.o

clean:
	rm -f *.o rmlayer.o

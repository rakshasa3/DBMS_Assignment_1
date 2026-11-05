CC = gcc
CFLAGS = -std=gnu89 -g -Wall -Wextra -Wno-implicit-function-declaration -Wno-implicit-int -Wno-builtin-declaration-mismatch -fno-builtin -I. -I../pflayer

all: rmlayer.o space_utilization

../pflayer/pflayer.o:
	$(MAKE) -C ../pflayer pflayer.o

rm.o: rm.c rm.h
	$(CC) $(CFLAGS) -c rm.c -o rm.o

rmlayer.o: rm.o
	ld -r -o rmlayer.o rm.o

space_utilization: space_utilization.c rm.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) space_utilization.c rm.o ../pflayer/pflayer.o -o space_utilization

clean:
	rm -f *.o rmlayer.o space_utilization courses_slotted.db

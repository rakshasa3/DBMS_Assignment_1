CC = gcc

CFLAGS = -std=gnu89 -g -Wall -Wextra -Wno-implicit-function-declaration -Wno-implicit-int -Wno-builtin-declaration-mismatch -fno-builtin -I. -I../pflayer

AM_OBJS = am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o

AMLAYER = ../amlayer/amlayer.o
PFLAYER = ../pflayer/pflayer.o

.PHONY: all clean

all: amlayer.o

amlayer.o: $(AM_OBJS)
	$(CC) -r -o $@ $(AM_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o a.out
cleanampf:
	rm -f ../pflayer/*.o ../amlayer/*.o *.o
pf:
	cd ../pflayer && make pflayer.o && cd ..
am:
	cd ../amlayer && make amlayer.o && cd ..

course_store.o: course_store.c course_store.h
	cc $(CFLAGS) -c course_store.c -o course_store.o

build_course_index.o: build_course_index.c course_store.h
	cc $(CFLAGS) -c build_course_index.c -o build_course_index.o

build_course_index: build_course_index.o course_store.o $(AMLAYER) $(PFLAYER)
	cc $(CFLAGS) build_course_index.o course_store.o $(AMLAYER) $(PFLAYER) -o build_course_index

verify_course_index.o: verify_course_index.c course_store.h
	cc $(CFLAGS) -c verify_course_index.c -o verify_course_index.o

verify_course_index: verify_course_index.o course_store.o $(AMLAYER) $(PFLAYER)
	cc $(CFLAGS) verify_course_index.o course_store.o $(AMLAYER) $(PFLAYER) -o verify_course_index

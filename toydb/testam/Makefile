CFLAGS = -I../amlayer -I../pflayer -std=gnu89 -Wno-implicit-function-declaration -Wno-implicit-int -Wno-builtin-declaration-mismatch
AMLAYER = ../amlayer/amlayer.o
PFLAYER = ../pflayer/pflayer.o

all: test1 test2 test3 test_data

misc.o: misc.c testam.h
	cc $(CFLAGS) -c misc.c

test1: test1.o misc.o $(AMLAYER) $(PFLAYER)
	cc $(CFLAGS) test1.o misc.o $(AMLAYER) $(PFLAYER) -o test1

test2: test2.o misc.o $(AMLAYER) $(PFLAYER)
	cc $(CFLAGS) test2.o misc.o $(AMLAYER) $(PFLAYER) -o test2

test3: test3.o misc.o $(AMLAYER) $(PFLAYER)
	cc $(CFLAGS) test3.o misc.o $(AMLAYER) $(PFLAYER) -o test3

test_data: test_data.o misc.o $(AMLAYER) $(PFLAYER)
	cc $(CFLAGS) test_data.o misc.o $(AMLAYER) $(PFLAYER) -o test_data

test1.o: test1.c testam.h
	cc $(CFLAGS) -c test1.c

test2.o: test2.c testam.h
	cc $(CFLAGS) -c test2.c

test3.o: test3.c testam.h
	cc $(CFLAGS) -c test3.c

test_data.o: test_data.c testam.h
	cc $(CFLAGS) -c test_data.c

clean:
	rm -f *.o test1 test2 test3 test_data testrel.* courserel.*
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

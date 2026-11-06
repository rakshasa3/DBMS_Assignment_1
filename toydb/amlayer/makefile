CC = gcc
CFLAGS = -std=gnu89 -g -Wall -Wextra -Wno-implicit-function-declaration -Wno-implicit-int -Wno-builtin-declaration-mismatch -fno-builtin -I. -I../pflayer

AM_OBJS = am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ambulkload.o

AMLAYER = amlayer.o
PFLAYER = ../pflayer/pflayer.o

TARGETS = benchmark_bulkload \
	  benchmark_student_bulkload \
          build_course_index \
          verify_course_index \
          build_course_index_incremental \
          build_student_index \
          build_student_index_incremental \
          

.PHONY: all clean cleanampf pf am

all: $(AMLAYER) $(TARGETS)

# Automatic rule to build pflayer dependency if missing
$(PFLAYER):
	$(MAKE) -C ../pflayer pflayer.o

amlayer.o: $(AM_OBJS)
	$(CC) -r -o $@ $(AM_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Header file dependencies
course_store.o: course_store.h
build_course_index.o: course_store.h
verify_course_index.o: course_store.h
build_course_index_incremental.o: course_store.h
student_store.o: student_store.h
build_student_index.o: student_store.h
build_student_index_incremental.o: student_store.h

# Executable Link Rules
benchmark_bulkload: benchmark_bulkload.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

build_course_index: build_course_index.o course_store.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

verify_course_index: verify_course_index.o course_store.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

build_course_index_incremental: build_course_index_incremental.o course_store.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

build_student_index: build_student_index.o student_store.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

build_student_index_incremental: build_student_index_incremental.o student_store.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

pf:
	$(MAKE) -C ../pflayer pflayer.o

am:
	$(MAKE) -C ../amlayer amlayer.o

clean:
	rm -f *.o a.out $(TARGETS)

cleanampf:
	rm -f ../pflayer/*.o ../amlayer/*.o *.o

benchmark_student_bulkload: benchmark_student_bulkload.o $(AMLAYER) $(PFLAYER)
	$(CC) $(CFLAGS) $^ -o $@

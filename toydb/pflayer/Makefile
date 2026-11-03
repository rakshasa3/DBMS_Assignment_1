#PUBLICDIR= /usr0/cs564/public/project
SRC= buf.c hash.c pf.c
OBJ= buf.o hash.o pf.o
HDR = pftypes.h pf.h 

#To overcome Compilation Errors because of legacy code
CC = gcc

CFLAGS = -std=gnu89 \
         -Wno-implicit-function-declaration \
         -Wno-implicit-int \
         -Wno-builtin-declaration-mismatch \
         -fno-builtin

pflayer.o: $(OBJ)
	ld -r -o pflayer.o $(OBJ)

tests: testhash testpf

testpf: testpf.o pflayer.o
	cc -o testpf testpf.o pflayer.o

testhash: testhash.o pflayer.o
	cc -o testhash testhash.o pflayer.o

$(OBJ): $(HDR)

testhash.o: $(HDR)

testpf.o: $(HDR)

lint: 
	lint $(SRC)

install: pflayer.o 


%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f *.o testpf testhash pflayer.o

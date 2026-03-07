# A simple executable that uses a data and function symbol from a library

CURRENT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

all: test

test: run-main

libtest.o: libtest.c
	${WCC} -shared -c libtest.c -o libtest.o

main.o: main.c
	${WCC} -c main.c -o main.o

libtest.so: libtest.o
	${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o libtest.so \
		libtest.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

main: main.o libtest.so
	${WLD} \
		-rpath ${CURRENT_DIR} \
		-dynamic-linker /lib64/ld-linux-x86-64.so.2 \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o main \
		main.o \
		-L . \
		-ltest \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		/usr/lib/x86_64-linux-gnu/crtn.o \

run-main: main
	./main
	echo "Minimal lib test passed"

clean:
	@rm -f *.o
	@rm -f *.so
	@rm -f main

# Test a R_X86_64_64 relocation in a shared library

WLD ?= ../../../bin/wld

# Use musl-wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/musl-wcc ] && echo ../../../../src/musl-wcc || echo gcc)

all: test

test: run-main

libtest.o: libtest.c
	gcc -c libtest.c -o libtest.o -fPIC

libtest.so: libtest.o
	${WLD}                                                \
	libtest.o                                             \
	-o libtest.so                                         \
	-L/usr/lib/x86_64-linux-musl                          \
	-lc                                                   \
	-shared

main.o: main.c
	gcc -c main.c -o main.o

main: main.o libtest.so
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main.o                                                \
	-o main                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-L .                                                  \
	-ltest                                                \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \


run-main: main
	LD_LIBRARY_PATH=. ./main

clean:
	@rm -f *.o
	@rm -f *.so
	@rm -f main

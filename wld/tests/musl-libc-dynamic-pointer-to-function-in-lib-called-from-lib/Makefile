# Test a case of a function pointer to a function in a library
# A R_X86_64_64 relocation must be present in .rela.dyn
# This tests the case of a main calling a library calling a library

WAS = ../../../bin/was
WLD ?= ../../../bin/wld

# Use musl-wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/musl-wcc ] && echo ../../../../src/musl-wcc || echo gcc)

all: test

test: run-main

libtest.so: libtest.c
	${CC} libtest.c -o libtest.so -shared

main.o: main.c
	${CC} -c main.c -o main.o

main: main.o libtest.so ${WLD}
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	main.o                                                \
	-o main                                               \
	-lc -lgcc -lgcc_eh                                    \
	-L. -ltest										      \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: main
	@LD_LIBRARY_PATH=. ./main
	@echo "Musl pointer to function in lib when called from lib passed"

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest.so

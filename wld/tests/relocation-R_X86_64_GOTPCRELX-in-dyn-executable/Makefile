# Test a GOTPCREL relocation in a call *foo@GOTPCREL(%rip) in a dynamic executable

WLD ?= ../../../bin/wld
WCC ?= ../../../../src/musl-wcc
GCC ?= musl-gcc

all: test

test: run-main

libtest.so: libtest.c
	${GCC} libtest.c -o libtest.so -shared

main.o: main.s
	as main.s -o main.o

main: main.o libtest.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	main.o                                                \
	-o main                                               \
	-lc -lgcc -lgcc_eh                                    \
	-L. -ltest                                            \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: main
	@LD_LIBRARY_PATH=. ./main
	@echo "Test GOTPCREL relocation in a call *foo@GOTPCREL(%rip) in a dynamic executable succeeded"

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest.so

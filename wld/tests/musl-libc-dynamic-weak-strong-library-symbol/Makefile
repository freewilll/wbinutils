# Test that a strong symbol in an .o file overrides a weak symbol in an .so file

WAS = ../../../bin/was
WLD = ../../../bin/wld

# Use musl-wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/musl-wcc ] && echo ../../../../src/musl-wcc || echo gcc)

all: test

test: run-main

%.o: %.s
	as $< -o $@

main.o: main.c
	AS=${WAS} ${CC} -c main.c -o main.o

libtest.o: libtest.s
	as -c libtest.s -o libtest.o

libtest.so: libtest.o ${WLD}
	${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o libtest.so \
		libtest.o \
		-L/usr/lib/x86_64-linux-musl \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

main: main.o libtest.so ${WLD}
	${WLD} \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	main.o                                                \
	-o main                                               \
 	-L .                                                  \
 	-ltest                                                \
	-lc -lgcc -lgcc_eh                                    \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o

run-main: main
	@LD_LIBRARY_PATH=. ./main

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest.so


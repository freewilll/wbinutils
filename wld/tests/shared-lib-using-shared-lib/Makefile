# There are three files with dependencies: main.c -> libtest1.c -> libtest2.c
# main is a dynamic executable and the libs .so files.
# This tests the GOT handling and entries in .rela.dyn for both libs.

WLD ?= ../../../bin/wld

# Use wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/wcc ] && echo ../../../../src/wcc || echo gcc)

CURRENT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

all: test

test: run-main

libtest1.o: libtest1.c
	${CC} -fPIC -c libtest1.c -o libtest1.o

libtest2.o: libtest2.c
	${CC} -fPIC -c libtest2.c -o libtest2.o

main.o: main.c
	${CC} -c main.c -o main.o

libtest1.so: libtest1.o libtest2.so ${WLD}
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o libtest1.so \
		libtest1.o \
		-L . \
		-ltest2 \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

libtest2.so: libtest2.o ${WLD}
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o libtest2.so \
		libtest2.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

main: main.o libtest1.so
	@LD_LIBRARY_PATH=. ${WLD} \
		-dynamic-linker /lib64/ld-linux-x86-64.so.2 \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o main \
		main.o \
		-L . \
		-ltest1 \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		/usr/lib/x86_64-linux-gnu/crtn.o \

run-main: main
	LD_LIBRARY_PATH=. ./main
	echo "Lib using lib test passed"

clean:
	@rm -f *.o
	@rm -f *.so
	@rm -f main

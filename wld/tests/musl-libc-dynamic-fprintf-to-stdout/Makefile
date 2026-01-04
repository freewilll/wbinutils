# This tests use case of wld adding a R_X86_64_COPY relocation
# due to main accessing an object in a library.
# This happens when accessing stdout in musl, which is also tested here

WAS = ../../../bin/was
WLD = ../../../bin/wld

# Use wcc if available, otherwise fall back to gcc
WCC ?= $(shell [ -x ../../../../src/wcc ] && echo ../../../../src/wcc || echo gcc)

all: test

test: run-main

main.o: main.c
	AS=${WAS} ${CC} -c main.c -o main.o

libtest.o: libtest.c
	AS=${WAS} ${CC} -c libtest.c -o libtest.o

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

main: main.o libtest.so
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main.o                                                \
	-o main                                               \
	-L .                                                  \
	-ltest                                                \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

run-main: main
	LD_LIBRARY_PATH=. ./main
	@MSG=$$(LD_LIBRARY_PATH=. ./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Musl hello world passed"
	echo "Using object in shared library and musl stdout passed"

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest.so

# Test a R_X86_64_64 relocation in a shared library

all: test

test: run-main

libtest.o: libtest.c
	${GCC} -c libtest.c -o libtest.o -fPIC

libtest.so: libtest.o
	${WLD}                                                \
	libtest.o                                             \
	-o libtest.so                                         \
	-L/usr/lib/x86_64-linux-musl                          \
	-lc                                                   \
	-shared

main.o: main.c
	${GCC} -c main.c -o main.o

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

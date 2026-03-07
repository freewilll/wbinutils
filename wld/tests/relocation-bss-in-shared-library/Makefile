# Test a dynamic executable that uses .bss symbols from a shared library

all: test

test: run-main

libtest1.o: libtest1.c
	gcc -c libtest1.c -o libtest1.o -fPIC

libtest2.o: libtest2.c
	gcc -c libtest2.c -o libtest2.o -fPIC

libtest1.so: libtest1.o
	${WLD}                                                \
	libtest1.o                                            \
	-o libtest1.so                                        \
	-L/usr/lib/x86_64-linux-musl                          \
	-lc                                                   \
	-shared

libtest2.so: libtest2.o
	${WLD}                                                \
	libtest2.o                                            \
	-o libtest2.so                                        \
	-L/usr/lib/x86_64-linux-musl                          \
	-lc                                                   \
	-shared

main.o: main.c
	gcc -c main.c -o main.o

main: main.o libtest1.so libtest2.so
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main.o                                                \
	-o main                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	libtest1.so                                           \
	libtest2.so                                           \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-pie

run-main: main
	LD_LIBRARY_PATH=. ./main

clean:
	@rm -f *.o
	@rm -f *.so
	@rm -f main

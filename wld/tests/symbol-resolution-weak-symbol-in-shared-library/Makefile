# Test a case originally found when dynamically linking sqlite3 with glibc.
# A library is built that calls stat(). stat() is declared as a weak hidden function
# in from an object file inside a library. This object file is linked
# both with the library and an executable that uses the library.
# wld must convert the weak symbol to a local symbol in the library.

WLD ?= ../../../bin/wld

all: run-main

main.o: main.c
	gcc -c main.c -o main.o

libtest1.o: libtest1.c
	gcc -c libtest1.c -o libtest1.o

libtest2.o: libtest2.c
	gcc -c libtest2.c -o libtest2.o

libtest1.so: libtest1.o libtest2.o
	${WLD} \
	-shared                            \
	-L /usr/lib                        \
	-L /usr/lib/gcc/x86_64-linux-gnu/9 \
	-o libtest1.so                     \
	libtest1.o                         \
	libtest2.o                         \
	-lc

main: main.o libtest1.so libtest2.o
	${WLD} \
	-dynamic-linker /lib64/ld-linux-x86-64.so.2  \
	-L /usr/lib                                  \
	-L /usr/lib/gcc/x86_64-linux-gnu/9           \
	-o main                                      \
	/usr/lib/x86_64-linux-gnu/crt1.o             \
	/usr/lib/x86_64-linux-gnu/crti.o             \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o  \
	main.o                                       \
	libtest1.so                                  \
	libtest2.o                                   \
	-lc                                          \
	-lgcc                                        \
	-lgcc_eh                                     \
	-lc                                          \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o    \
	/usr/lib/x86_64-linux-gnu/crtn.o             \
	-pie

run-main: main
	LD_LIBRARY_PATH=. ./main

clean:
	rm -f main
	rm -f *.o
	rm -f *.so

# Check that a symbol in a dynamic executable preempts the same symbol in a shared library

all: run-main

main.o: main.c
	${WCC} -c main.c -o main.o

libtest1a.o: libtest1.c
	${WCC} -c libtest1.c -o libtest1a.o -fPIC

libtest1b.o: libtest1.c
	${WCC} -c libtest1.c -o libtest1b.o -fPIC

libtest1.so: libtest1a.o
	${WLD} \
	-shared                            \
	-L /usr/lib                        \
	-L /usr/lib/gcc/x86_64-linux-gnu/9 \
	-o libtest1.so                     \
	libtest1a.o                        \
	-lc


main: main.o libtest1.so libtest1b.o
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
	-rpath .                                     \
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

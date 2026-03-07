# Test some scenarios with a common symbol in an object file that
# is resolved by a symbol in a shared library that can be in the .bss or .data.

all: run-main

main.o: main.c
	${GCC} -c main.c -o main.o

libtest1.o: libtest1.c
	${GCC} -c libtest1.c -o libtest1.o -fPIC

libtest2.o: libtest2.c
	${GCC} -c libtest2.c -o libtest2.o -fPIC

libtest3.o: libtest3.c
	${GCC} -c libtest3.c -o libtest3.o -fPIC

libtest1.so: libtest1.o libtest2.o libtest3.o
	${WLD} \
	-shared                            \
	-L /usr/lib                        \
	-L /usr/lib/gcc/x86_64-linux-gnu/9 \
	-o libtest1.so                     \
	libtest1.o                         \
	libtest2.o                         \
	libtest3.o                         \
	-lc

main: main.o libtest1.so
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
	./main

clean:
	rm -f main
	rm -f *.o
	rm -f *.so

# Test a case where two .so files are used by an executable
# .so1 defines f() as a weak non-default symbol
# .so2 defines f() as a strong default symbol
# The libs are loaded as -ltest1, -ltest2, main.o
# This tests a bug where the weak default symbol would not resolve the symbol in main.

WLD ?= ../../../bin/wld
WCC ?= ../../../../src/musl-wcc
GCC ?= musl-gcc

all: test

test: run-main

main.o: main.c
	${GCC} -c main.c -o main.o

libtest1.so: libtest1.c libtest1.map
	${GCC} libtest1.c -o libtest1.so -shared -Wl,--version-script=libtest1.map

libtest2.so: libtest2.o libtest2.map
	${GCC} libtest2.c -o libtest2.so -shared -Wl,--version-script=libtest2.map

main: main.o libtest1.so libtest2.so
	@# Note the specfific order: -ltest2, -ltest1, main.o

	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	main.o                                                \
	-o main                                               \
	-lc -lgcc -lgcc_eh                                    \
	-L.                                                   \
	-ltest1                                               \
	-ltest2                                               \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: main
	@LD_LIBRARY_PATH=. ./main
	@echo "Versioned strong-non-default and weak-default symbols in two libs"

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest1.so
	@rm -f libtest2.so

# Symbol versioning test
# Four symbols are exported in a lib: f1@V1, f1@V2 and f2@V1 and f2@V2.
# f1 has a default version V1 and f2 a default version V2.

all: test

test: run-main

libtest.so: libtest.c libtest.map
	${MUSL_GCC} libtest.c -o libtest.so -shared -Wl,--version-script=libtest.map

main.o: main.c
	${MUSL_WCC} -c main.c -o main.o

main: main.o libtest.so
	${WLD}                                                \
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
	@echo "Versioned symbol defaults test passed"

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest.so

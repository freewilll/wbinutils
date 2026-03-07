# Deal with a special case of an undefined symbol being:
# - in a library as f1@V1
# - in an object file as f1
# The undefined f1 is seen first, then the undefined f1@V1, then the defined f1@@V1 which resolves both.

all: test

test: run-main

main.o: main.c
	${GCC} -c main.c -o main.o

libtest2.o: libtest2.c
	${GCC} -c libtest2.c -o libtest2.o

libtest1.so: libtest1.c libtest1.map
	${GCC} libtest1.c -o libtest1.so -shared -Wl,--version-script=libtest1.map

libtest2.so: libtest2.o
	@${WLD} -shared -L /usr/lib/x86_64-linux-musl -o libtest2.so -L . libtest2.o -ltest1 -lc


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
	-ltest2                                               \
	-ltest1                                               \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: main
	@LD_LIBRARY_PATH=. ./main
	@echo "Versioned symbol double undefined with default version test passed"

clean:
	@rm -f *.o
	@rm -f main
	@rm -f libtest1.so
	@rm -f libtest2.so

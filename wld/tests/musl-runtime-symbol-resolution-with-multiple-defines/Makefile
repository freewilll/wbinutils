# Test two cases:
# 1. An object file defines a variable. Two libraries also define it, but the definition in the object file comes first
# 2. Two library files define a variable. The first definition comes first. The order is tested.

all: test

test: run-main-with-def run-main-no-def-test1-first run-main-no-def-test2-first

main-no-def.o: main-no-def.c
	${GCC} -c -o main-no-def.o main-no-def.c

main-with-def.o: main-with-def.c
	${GCC} -c -o main-with-def.o main-with-def.c

libtest1.o: libtest1.c
	${GCC} -c libtest1.c -o libtest1.o -g -fPIC

libtest2.o: libtest2.c
	${GCC} -c libtest2.c -o libtest2.o -g -fPIC

libtest1.so: libtest1.o
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o libtest1.so \
		libtest1.o \
		-L/usr/lib/x86_64-linux-musl \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

libtest2.so: libtest1.so libtest2.o
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o libtest2.so \
		libtest2.o \
		-L/usr/lib/x86_64-linux-musl \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		-L . \
		-ltest1

main-with-def: main-with-def.o libtest1.so libtest2.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main-with-def.o                                       \
	-o main-with-def                                      \
	-L .                                                  \
	-ltest1                                               \
	-ltest2                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

main-no-def-test1-first: main-no-def.o libtest1.so libtest2.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main-no-def.o                                         \
	-o main-no-def-test1-first                            \
	-L .                                                  \
	-ltest1                                               \
	-ltest2                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

main-no-def-test2-first: main-no-def.o libtest1.so libtest2.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main-no-def.o                                         \
	-o main-no-def-test2-first                            \
	-L .                                                  \
	-ltest2                                               \
	-ltest1                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

run-main-with-def: main-with-def
	LD_LIBRARY_PATH=. ./main-with-def; \
	test $$? -eq 1 || { echo "Exit code was $$?, not 1"; exit 1; }
	@echo "Runtime symbol resolution with-def passed"

run-main-no-def-test1-first: main-no-def-test1-first
	LD_LIBRARY_PATH=. ./main-no-def-test1-first; \
	test $$? -eq 2 || { echo "Exit code was $$?, not 2"; exit 1; }
	@echo "Runtime symbol resolution no-def-test1 passed"

run-main-no-def-test2-first: main-no-def-test2-first
	LD_LIBRARY_PATH=. ./main-no-def-test2-first; \
	test $$? -eq 3 || { echo "Exit code was $$?, not 3"; exit 1; }
	@echo "Runtime symbol resolution no-def-test2 passed"

clean:
	rm -f *.so
	rm -f *.o
	rm -f main-with-def
	rm -f main-no-def-test1-first
	rm -f main-no-def-test2-first

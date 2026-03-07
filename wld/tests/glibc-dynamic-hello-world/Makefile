WAS = ../../../bin/was
WLD = ../../../bin/wld

# Use wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/wcc ] && echo ../../../../src/wcc || echo gcc)

all: test

test: run-main

main.o: main.c
	AS=${WAS} ${CC} -c main.c -o main.o -g

main: main.o
	${WLD} \
	-dynamic-linker /lib64/ld-linux-x86-64.so.2  \
	-L /usr/lib                                  \
	-L /usr/lib/gcc/x86_64-linux-gnu/9           \
	-o main                                      \
	/usr/lib/x86_64-linux-gnu/crt1.o             \
	/usr/lib/x86_64-linux-gnu/crti.o             \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o  \
	main.o                                       \
	-lc                                          \
	-lgcc                                        \
	-lgcc_eh                                     \
	-lc                                          \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o    \
	/usr/lib/x86_64-linux-gnu/crtn.o \

run-main: main
	@./main # Catch non-zero error
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\

	## Ensure ld-linux isn't present in .dynamic section NEEDED
	@! readelf -dW main | grep NEEDED | grep ld-linux

	echo "Glibc dynamic hello world passed"

clean:
	@rm -f *.o
	@rm -f main

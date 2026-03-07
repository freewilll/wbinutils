# Test a case of a function pointer to a function in a library
# A R_X86_64_64 relocation must be present in .rela.dyn

WAS = ../../../bin/was
WLD ?= ../../../bin/wld

# Use musl-wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/musl-wcc ] && echo ../../../../src/musl-wcc || echo gcc)

all: test

test: run-main

%.o: %.s
	${WAS} $< -o $@

main.o: main.c
	AS=${WAS} ${CC} -c main.c -o main.o

main: main.o ${WLD}
	${WLD} \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	main.o                                                \
	-o main                                               \
	-lc -lgcc -lgcc_eh                                    \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: main
	@./main
	echo "Musl pointer to function in lib passed"

clean:
	@rm -f *.o
	@rm -f main

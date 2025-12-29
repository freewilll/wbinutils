WAS = ../../../bin/was
WLD = ../../../bin/wld

# Use wcc if available, otherwise fall back to gcc
CC = $(shell [ -x ../../../../src/wcc ] && echo ../../../../src/wcc || echo gcc)

all: test

test: run-main

%.o: %.s
	as $< -o $@

main.o: main.c
	AS=${WAS} ${CC} -c main.c -o main.o

main: main.o
	${WLD} \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-static \
		-o main \
		main.o \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		-lm \
		/usr/lib/x86_64-linux-gnu/crtn.o \
		-static

run-main: main
	@./main
	echo "Glibc math passed"

clean:
	@rm -f *.o
	@rm -f main

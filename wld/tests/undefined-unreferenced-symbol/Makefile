# A symbol may be undefined so long as it isn't used.
# This happens with __pthread_create in glibc

all: test

test: run-main

%.o: %.s
	${WAS} $< -o $@

main.o: main.c
	${GCC} -c main.c -o main.o

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
		/usr/lib/x86_64-linux-gnu/crtn.o

run-main: main
	@./main
	echo "Undefined unreferenced symbol test passed"

clean:
	@rm -f *.o
	@rm -f main

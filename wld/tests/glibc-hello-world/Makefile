WCC = ../../../../src/wcc
WAS = ../../../bin/was
WLD = ../../../bin/wld

all: main

%.o: %.s
	as $< -o $@

main.o: main.c
	AS=${WAS} ${WCC} -c main.c -o main.o

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

clean:
	@rm -f *.o
	@rm -f main

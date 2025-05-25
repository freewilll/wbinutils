WCC = ../../../../src/wcc
WAS = ../../../bin/was
WLD = ../../../bin/wld

all: run-main

%.o: %.s
	as $< -o $@

main.o: main.c
	AS=${WAS} ${WCC} -c main.c -o main.o

main: main.o
	ld \
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
	@./main # Catch non-zero error
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Libc with gcc ld test passed"

clean:
	@rm -f *.o
	@rm -f main

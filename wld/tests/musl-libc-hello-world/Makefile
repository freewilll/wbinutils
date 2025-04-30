WCC = ../../../../src/wcc
WAS = ../../../bin/was
WLD = ../../../bin/wld

test: all

all: run-main

%.o: %.s
	as $< -o $@

main.o: main.c
	# AS=${WAS} ${WCC} -c main.c -o main.o # TODO
	gcc main.c -c -o main.o

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
	/usr/lib/x86_64-linux-musl/crtn.o

run-main: main
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Musl hello world passed"

clean:
	@rm -f *.o
	@rm -f main

WLD = ../../../bin/wld

all: test

test: run-main

%.o: %.s
	as $< -o $@

main: ${WLD} main.o libc.o
	${WLD} -o main main.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o -static

run-main: main
	./main
	echo "relocation-relaxation test passed"

clean:
	@rm -f *.o
	@rm -f main

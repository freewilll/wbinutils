WLD = ../../../bin/wld

all: test

test: run-main

%.o: %.s
	as $< -o $@

main: ${WLD} main.o
	${WLD} -o main main.o -static

run-main: main
	./main

clean:
	@rm -f *.o
	@rm -f main

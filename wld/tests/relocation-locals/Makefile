WLD = ../../../bin/wld

all: test

test: run-main

%.o: %.s
	as $< -o $@

main: main.o lib1.o lib2.o lib3.o
	${WLD} -o main main.o lib1.o lib2.o lib3.o

run-main: main
	./main

clean:
	@rm -f *.o
	@rm -f main

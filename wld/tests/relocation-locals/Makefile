all: test

test: run-main

%.o: %.s
	${AS} $< -o $@

main: ${WLD} main.o lib1.o lib2.o lib3.o
	${WLD} -o main main.o lib1.o lib2.o lib3.o -static

run-main: main
	./main

clean:
	@rm -f *.o
	@rm -f main

all: test

test: run-main

%.o: %.s
	${AS} $< -o $@

main: main.o ${WLD}
	${WLD} main.o -o main -static

run-main: main
	./main

clean:
	@rm -f *.o
	@rm -f main

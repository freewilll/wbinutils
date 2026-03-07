# Test a bug where an undefined weak symbol doesn't get upgraded to strong

all: test

test: run-main

%.o: %.c
	${GCC} $< -c -o $@

%.o: %.s
	${AS} $< -o $@

liba.a: test1.o test2.o test3.o
	ar rcs $@ test1.o test2.o test3.o

main: liba.a main.o ${WLD}
	${WLD} main.o -L . -la -o main -static

run-main: main
	./main

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f main

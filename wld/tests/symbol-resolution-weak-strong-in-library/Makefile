# Test a bug where an undefined weak symbol doesn't get upgraded to strong

WLD = ../../../bin/wld

all: test

test: run-main

%.o: %.c
	gcc $< -c -o $@

%.o: %.s
	as $< -o $@

liba.a: test1.o test2.o test3.o
	ar rcs $@ test1.o test2.o test3.o

main: liba.a main.o ${WLD}
	${WLD} main.o -L . -la -o main

run-main: main
	./main

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f main

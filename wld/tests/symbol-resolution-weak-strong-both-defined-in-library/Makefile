# A library has one symbol defined twice: weak, then strong.
# If the second object file is included, the strong symbol takes over the weak.

all: test

test: run-main12 run-main21

%.o: %.c
	${GCC} $< -c -o $@

%.o: %.s
	${AS} $< -o $@

liba12.a: test1.o test2.o
	ar rcs $@ test1.o test2.o

liba21.a: test1.o test2.o
	ar rcs $@ test2.o test1.o

main12: liba12.a main.o
	${WLD} main.o -L . -la12 -o main12 -static

main21: liba21.a main.o
	${WLD} main.o -L . -la21 -o main21 -static

run-main12: main12
	./main12

run-main21: main21
	./main21

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f main12
	@rm -f main21


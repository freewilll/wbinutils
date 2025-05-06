WLD = ../../../bin/wld

all: test

test: run-oddballs

%.o: %.s
	as $< -o $@

oddballs: oddballs.o oddballs2.o ${WLD}
	${WLD} oddballs.o oddballs2.o -o oddballs

run-oddballs: oddballs
	./oddballs

clean:
	@rm -f *.o
	@rm -f oddballs

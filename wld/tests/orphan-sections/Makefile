WLD ?= ../../../bin/wld

all: test

test: run-orphans

%.o: %.s
	as $< -o $@

orphans: orphans.o orphans2.o ${WLD}
	${WLD} orphans.o orphans2.o -o orphans -static

run-orphans: orphans
	./orphans

clean:
	@rm -f *.o
	@rm -f orphans

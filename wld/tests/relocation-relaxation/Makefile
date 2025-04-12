WLD = ../../../bin/wld

all: test

test: main

%.o: %.s
	as $< -o $@

main: main.o libc.o
	${WLD} -o main main.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o
	# gcc -o main main.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o -nostdlib -static # wwip

	@MSG=$$(./main); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	echo "relocation-relaxation test passed"

clean:
	@rm -f *.o
	@rm -f main

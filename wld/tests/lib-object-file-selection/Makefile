WLD = ../../../bin/wld

all: test

test: run-main

%.o: %.s
	as $< -o $@

lib1.a: lib1.o lib2.o lib3.o
	ar rcs lib1.a lib1.o lib2.o lib3.o

main:  main.o lib1.a ${WLD}
	${WLD} main.o -L. -l1 -o main

run-main: main
	@MSG=$$(./main); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection test passed"

clean:
	@rm -f *.o
	@rm -f lib1.a
	@rm -f main

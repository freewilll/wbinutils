WLD ?= ../../../bin/wld

all: test

test: run-main run-main-with-optional

%.o: %.s
	as $< -o $@

main: main.o
	${WLD} -o main main.o -static

run-main: main
	./main
	echo "relocation-got test passed"

main-with-optional: main.o optional.o
	${WLD} -o main-with-optional main.o optional.o -static

run-main-with-optional: main-with-optional
	@./main-with-optional; ERR=$$? ;\
	if [ "$$ERR" != "2" ]; then \
		echo "Exit code was $$ERR, not 2."; \
		exit 1; \
	else \
		echo "relocation-got with optional test passed"; \
	fi

clean:
	@rm -f *.o
	@rm -f main

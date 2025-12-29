WLD = ../../../bin/wld

all: test

test: run-test1

%.o: %.s
	as $< -o $@

test1: test1.o ${WLD}
	${WLD} test1.o -o test1 -static

run-test1: test1
	@./test1; ERR=$$? ;\
	if [ "$$ERR" != "7" ]; then \
		echo "Exit code was $$ERR, not 7."; \
		exit 1; \
	else \
		echo "symbol-resolution-with-mixed-bss-and-common-sections test passed"; \
	fi

clean:
	@rm -f *.o
	@rm -f test1

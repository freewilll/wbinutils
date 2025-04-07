WLD = ../../../bin/wld

all: run-test1

%.o: %.s
	as $< -o $@

test1: test1.o ${WLD}
	${WLD} test1.o -o test1

run-test1: test1
	@./test1; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Local symbols test passed"; \
	fi

clean:
	@rm -f *.o
	@rm -f test1

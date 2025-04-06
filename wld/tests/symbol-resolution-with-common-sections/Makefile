WLD = ../../../bin/wld

all: run

test: run

%.o: %.s
	as $< -o $@

file: ${WLD} file1.o file2.o
	${WLD} file1.o file2.o -o file

run: file
	@MSG=$$(./file); ERR=$$? ;\
	if [ "$$ERR" != "10" ]; then \
		echo "Exit code was $$ERR, not 10."; \
		exit 1; \
	fi ;\
	echo "Common sections test passed"

clean:
	@rm -f *.o
	@rm -f file

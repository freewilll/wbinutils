WLD = ../../../bin/wld

all: run-sanity1

%.o: %.s
	as $< -o $@

sanity1: sanity1.o ${WLD}
	${WLD} sanity1.o -o sanity1

run-sanity1: sanity1
	@./sanity1; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Sanity 1 test passed"; \
	fi

clean:
	@rm -f *.o
	@rm -f sanity1

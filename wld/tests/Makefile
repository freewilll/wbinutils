WLD = ../../bin/wld

all: run-sanity1 run-sanity2

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

sanity2: sanity2a.o sanity2b.o ${WLD}
	# ${WLD} sanity2a.o sanity2b.o -o sanity2 # TODO make wld do the linking
	ld sanity2a.o sanity2b.o -o sanity2

run-sanity2: sanity2
	@./sanity2; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Sanity 2 test passed"; \
	fi
clean:
	@rm -f sanity1.o
	@rm -f sanity1
	@rm -f sanity2
	@rm -f sanity2a.o
	@rm -f sanity2b.o

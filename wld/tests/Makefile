WLD = ../../bin/wld

all: run-sanity

# Sanity test
sanity.o: sanity.s
	as sanity.s -o sanity.o

sanity: sanity.o ${WLD}
	${WLD} sanity.o -o sanity

run-sanity: sanity
	@./sanity; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Sanity test passed"; \
	fi

clean:
	@rm -f sanity.o
	@rm -f sanity
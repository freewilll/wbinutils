WCC = ../../../src/wcc
WAS = ../../bin/was
WLD = ../../bin/wld

all: run-sanity1 run-sanity2ab run-sanity2ba run-sanity3

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

sanity2ab: sanity2a.o sanity2b.o ${WLD}
	${WLD} sanity2a.o sanity2b.o -o sanity2ab

sanity2ba: sanity2a.o sanity2b.o ${WLD}
	${WLD} sanity2b.o sanity2a.o -o sanity2ba

run-sanity2ab: sanity2ab
	@MSG=$$(./sanity2ab); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 2ab test passed"

run-sanity2ba: sanity2ba
	@MSG=$$(./sanity2ba); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 2ba test passed"

sanity3.o: sanity3.c
	AS=${WAS} ${WCC} -c sanity3.c -o sanity3.o

sanity3: sanity3.o
	ld \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-static \
		-o sanity3 \
		sanity3.o \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		/usr/lib/x86_64-linux-gnu/crtn.o

run-sanity3: sanity3
	@MSG=$$(./sanity3);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 3 test passed"

clean:
	@rm -f *.o
	@rm -f sanity1
	@rm -f sanity2ab
	@rm -f sanity2ba
	@rm -f sanity3

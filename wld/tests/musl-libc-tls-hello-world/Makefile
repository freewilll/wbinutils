WCC = ../../../../src/wcc
WAS = ../../../bin/was
WLD = ../../../bin/wld

# A couple of tests of executables that have combinations of .tdata and.or .tbss sections.

test: all

all:  \
	run-main-data \
	run-main-bss \
	run-main-data-and-bss

%.o: %.c
	gcc -c $< -o $@

%-data: %-data.o ${WLD}
	@${WLD} \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	$<                                                    \
	-o $@                                                 \
	-lc -lgcc -lgcc_eh                                    \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o

.PHONY: run-main-data
run-main-data: main-data
	@./main-data # Catch non-zero error
	@MSG=$$(./main-data);\
	if [ "$$MSG" != "Hello World! 42" ]; then \
		echo "Message was not Hello World! 42"; \
		exit 1; \
	fi ;\
	echo "Musl hello world data passed"

.PHONY: run-main-bss
run-main-bss: main-bss
	@./main-bss # Catch non-zero error exit code
	@MSG=$$(./main-bss);\
	if [ "$$MSG" != "Hello World! 0" ]; then \
		echo "Message was not Hello World! 0"; \
		exit 1; \
	fi ;\
	echo "Musl hello world bss passed"

.PHONY: run-main-bss
run-main-data-and-bss: main-data-and-bss
	@./main-data-and-bss # Catch non-zero error exit code
	@MSG=$$(./main-data-and-bss);\
	if [ "$$MSG" != "Hello World! 42" ]; then \
		echo "Message was not Hello World! 42"; \
		exit 1; \
	fi ;\
	echo "Musl hello world data and bss passed"

clean:
	@rm -f *.o
	@rm -f main-data
	@rm -f main-bss
	@rm -f main-data-and-bss

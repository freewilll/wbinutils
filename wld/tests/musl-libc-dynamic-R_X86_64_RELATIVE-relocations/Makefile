# Test the conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables

all: test

test: run-main

main.o: main.c
	${MUSL_WCC} -c main.c -o main.o

main: main.o
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	main.o                                                \
	-o main                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

run-main: main
	 ./main
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables passed"

clean:
	@rm -f *.o
	@rm -f main

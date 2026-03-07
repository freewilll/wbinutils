# Test the TLS segment size not being a multiple of the aligment.
# This caused a bug where the start of the TLS section with respect to the thread pointer
# was incorrect since it has to be a multiple of the aligmnment.

all: run-main

%.o: %.c
	@# Use gcc since the source files have __thread which wcc doesn't support
	${GCC} -c $< -o $@

# Note the unusual placement of main.o at the end. This causes the TLS segment to be sized a multiple of 4
main: main.o test.o
	@${WLD} \
	-static \
	-L \
	/usr/lib \
	-L \
	/usr/lib/gcc/x86_64-linux-gnu/9 \
	-o main \
	/usr/lib/x86_64-linux-gnu/crt1.o \
	/usr/lib/x86_64-linux-gnu/crti.o \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o \
	-lc \
	-lgcc \
	-lgcc_eh \
	test.o \
	-lc /usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o \
	/usr/lib/x86_64-linux-gnu/crtn.o \
	main.o

.PHONY: run-main
run-main: main
	@./main
	@echo glibc-tls-alignment test passed

clean:
	@rm -f *.o
	@rm -f main

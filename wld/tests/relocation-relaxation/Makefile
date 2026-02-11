WLD ?= ../../../bin/wld

all: test

test: run-main-static run-main-dynamic

main-static.o: main-static.s
	# Use -mrelax-relocations=no to get a R_X86_64_GOTPCREL, otherwise gas makes a R_X86_64_GOTPCRELX
	as $< -o $@

main-dynamic.o: main-dynamic.s
	# Use -mrelax-relocations=no to get a R_X86_64_GOTPCREL, otherwise gas makes a R_X86_64_GOTPCRELX
	as $< -o $@ -mrelax-relocations=no

main-static: main-static.o libc.o
	${WLD} -o main-static main-static.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o -static

main-dynamic: main-dynamic.o libc.o
	${WLD} -o main-dynamic main-dynamic.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o -dynamic-linker /lib/ld-musl-x86_64.so.1

run-main-static: main-static
	./main-static
	echo "relocation-relaxation static test passed"

run-main-dynamic: main-dynamic
	./main-dynamic
	echo "relocation-relaxation dynamic test passed"

clean:
	@rm -f *.o
	@rm -f main-static
	@rm -f main-dynamic


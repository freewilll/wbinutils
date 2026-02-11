WLD ?= ../../../bin/wld

all: test

test: run-main-static-relaxed run-main-static-unrelaxed run-main-dynamic

main-static-relaxed.o: main-static.s
	# gas makes R_X86_64_GOTPCRELX relocations for the GOTPCREL instructions
	as $< -o $@

main-static-unrelaxed.o: main-static.s
	# gas makes R_X86_64_GOTPCREL relocations for the GOTPCREL instructions
	as $< -o $@ -mrelax-relocations=no

main-dynamic.o: main-dynamic.s
	# Use -mrelax-relocations=no to get a R_X86_64_GOTPCREL, otherwise gas makes a R_X86_64_GOTPCRELX
	as $< -o $@ -mrelax-relocations=no

main-static-relaxed: main-static-relaxed.o libc.o
	${WLD} -o main-static-relaxed main-static-relaxed.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o -static

main-static-unrelaxed: main-static-unrelaxed.o libc.o
	${WLD} -o main-static-unrelaxed main-static-unrelaxed.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o -static

main-dynamic: main-dynamic.o libc.o
	${WLD} -o main-dynamic main-dynamic.o libc.o /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o -dynamic-linker /lib/ld-musl-x86_64.so.1

run-main-static-relaxed: main-static-relaxed
	./main-static-relaxed
	echo "relocation-relaxation static relaxed test passed"

run-main-static-unrelaxed: main-static-unrelaxed
	./main-static-unrelaxed
	echo "relocation-relaxation static unrelaxed test passed"

run-main-dynamic: main-dynamic
	./main-dynamic
	echo "relocation-relaxation dynamic test passed"

clean:
	@rm -f *.o
	@rm -f main-static-relaxed
	@rm -f main-static-unrelaxed
	@rm -f main-dynamic

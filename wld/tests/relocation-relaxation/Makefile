WLD ?= ../../../bin/wld

all: test

test: run-main-static-relaxed run-main-static-unrelaxed run-main-dynamic-relaxed run-main-dynamic-unrelaxed

main-static-relaxed.o: main.s
	# gas makes R_X86_64_GOTPCRELX relocations for the GOTPCREL instructions
	as $< -o $@

main-static-unrelaxed.o: main.s
	# gas makes R_X86_64_GOTPCREL relocations for the GOTPCREL instructions
	as $< -o $@ -mrelax-relocations=no

main-dynamic-relaxed.o: main.s
	# gas makes R_X86_64_GOTPCRELX relocations for the GOTPCREL instructions
	as $< -o $@

main-dynamic-unrelaxed.o: main.s
	# gas makes R_X86_64_GOTPCREL relocations for the GOTPCREL instructions
	as $< -o $@ -mrelax-relocations=no

main-static-relaxed: main-static-relaxed.o libtest.o
	${WLD}                                                \
	-static                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	main-static-relaxed.o                                 \
	libtest.o                                             \
	-o main-static-relaxed                                \
	-lc -lgcc -lgcc_eh                                    \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \

main-static-unrelaxed: main-static-unrelaxed.o libtest.o
	${WLD}                                                \
	-static                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o           \
	-L/usr/lib/x86_64-linux-musl                          \
	-L/usr/lib/gcc/x86_64-linux-gnu/9                     \
	libtest.o                                             \
	main-static-unrelaxed.o                               \
	-o main-static-unrelaxed                              \
	-lc -lgcc -lgcc_eh                                    \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o             \
	/usr/lib/x86_64-linux-musl/crtn.o                     \

main-dynamic-relaxed: main-dynamic-relaxed.o libtest.o
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	libtest.o                                             \
	main-dynamic-relaxed.o                                \
	-o main-dynamic-relaxed                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-pie                                                  \

main-dynamic-unrelaxed: main-dynamic-unrelaxed.o libtest.o
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	libtest.o                                             \
	main-dynamic-unrelaxed.o                              \
	-o main-dynamic-unrelaxed                             \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-pie                                                  \

run-main-static-relaxed: main-static-relaxed
	./main-static-relaxed
	echo "relocation-relaxation static relaxed test passed"

run-main-static-unrelaxed: main-static-unrelaxed
	./main-static-unrelaxed
	echo "relocation-relaxation static unrelaxed test passed"

run-main-dynamic-relaxed: main-dynamic-relaxed
	./main-dynamic-relaxed
	echo "relocation-relaxation dynamic relaxed test passed"

run-main-dynamic-unrelaxed: main-dynamic-unrelaxed
	./main-dynamic-unrelaxed
	echo "relocation-relaxation dynamic unrelaxed test passed"

clean:
	@rm -f *.o
	@rm -f main-static-relaxed
	@rm -f main-static-unrelaxed
	@rm -f main-dynamic-relaxed
	@rm -f main-dynamic-unrelaxed

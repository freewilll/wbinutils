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

main-dynamic: main-dynamic.o libtest.o
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	libtest.o                                             \
	main-dynamic.o                                        \
	-o main-dynamic                                       \
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

run-main-dynamic: main-dynamic
	./main-dynamic
	echo "relocation-relaxation dynamic test passed"

clean:
	@rm -f *.o
	@rm -f main-static-relaxed
	@rm -f main-static-unrelaxed
	@rm -f main-dynamic

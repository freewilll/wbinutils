# These tests also have much more concise unit tests,
# so are redundant. But they are left here anyways, since
# they are good e2e tests.

WLD = ../../../bin/wld

all: test

test: \
	run-main_o1_o2 \
	run-main_l1_o2 \
	run-main_o1_l2 \
	run-main_l1_l2 \
	run-main_o2_o1 \
	run-main_l2_o1 \
	run-main_o2_l1 \
	run-main_l2_l1 \

# Test a weak symbol in file 1 and a strong symbol in file 2.
# In all cases, the object files should link and the strong symbol
# takes precedence over the weak.

%.o: %.c
	gcc $< -c -o $@

%.o: %.s
	as $< -o $@

%.a: %.o
	ar rcs $@ $<

main_o1_o2: main.o lib1.o lib2.o ${WLD}
	${WLD} main.o lib1.o lib2.o -o main_o1_o2

main_l1_o2: main.o lib1.a lib2.o ${WLD}
	${WLD} main.o -L . -l1 lib2.o -o main_l1_o2

main_o1_l2: main.o lib1.o lib2.a ${WLD}
	${WLD} main.o lib1.o -L . -l2 -o main_o1_l2

main_l1_l2: main.o lib1.a lib2.a ${WLD}
	${WLD} main.o -L . -l1 -l2 -o main_l1_l2

main_o2_o1: main.o lib1.o lib2.o ${WLD1}
	${WLD} main.o lib2.o lib1.o -o main_o2_o1

main_o2_l1: main.o lib1.a lib2.o ${WLD}
	${WLD} main.o lib2.o -L . -l1 -o main_o2_l1

main_l2_o1: main.o lib1.o lib2.a ${WLD}
	${WLD} main.o -L . -l2 lib1.o -o main_l2_o1

main_l2_l1: main.o lib1.a lib2.a ${WLD}
	${WLD} main.o -L . -l2 -l1 -o main_l2_l1

run-main_%: main_%
	@./$<
	@echo "lib-object-file-selection $< test passed"

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f main_*


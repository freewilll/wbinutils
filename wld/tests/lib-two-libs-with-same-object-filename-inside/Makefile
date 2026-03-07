# Test a regression related to two static libs that have the same filename in it, with local symbols
# The looking for local symbol tables was based on filename alone instead of a combination of libname and filename.

all: test

test: run-main

lib1dir:
	mkdir lib1dir

lib2dir:
	mkdir lib2dir

lib1dir/lib.o: lib1.s lib1dir
	${AS} -o lib1dir/lib.o lib1.s

lib2dir/lib.o: lib2.s lib2dir
	${AS} -o lib2dir/lib.o lib2.s

lib1.a: lib1.o lib1dir/lib.o
	ar rcs lib1.a lib1dir/lib.o

lib2.a: lib2.o lib2dir/lib.o
	ar rcs lib2.a lib2dir/lib.o

main:  main.o lib1.a lib2.a ${WLD}
	${WLD} main.o -L. -l1 -l2 -o main -static

run-main: main
	@./main
	echo "lib-two-libs-with-same-object-filename-inside"

clean:
	@rm -Rf lib1dir
	@rm -Rf lib2dir
	@rm -f *.o
	@rm -f *.a
	@rm -f main

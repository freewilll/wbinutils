# These tests also have much more concise unit tests,
# so are redundant. But they are left here anyways, since
# they are good e2e tests.

WLD = ../../../bin/wld

all: test

test: main_o1_o2 main_l1_o2 run-main_o1_l2 run-main_l1_l2

# Test two strong symbols
# obj1 obj2 fail
# lib1 obj2 fail
# obj1 lib2 ok
# lib1 lib2 ok

%.o: %.c
	gcc $< -c -o $@

%.o: %.s
	as $< -o $@

%.a: %.o
	ar rcs $@ $<

main_o1_o2: main.o lib1.o lib2.o ${WLD}
	@MSG=$$(${WLD} main.o lib1.o lib2.o -o main_o1_o2 2>/dev/null); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "wld exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection wld exit code for main_o1_o2 test passed"

main_l1_o2: main.o lib1.a lib2.o ${WLD}
	@MSG=$$(${WLD} main.o -L . -l1 lib2.o -o main_l1_o2 2>/dev/null); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "wld exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection wld exit code for main_l1_o2 test passed"

main_o1_l2: main.o lib1.o lib2.a ${WLD}
	# lib1.a then lib2.o
	${WLD} main.o lib1.o -L . -l2 -o main_o1_l2

main_l1_l2: main.o lib1.a lib2.a ${WLD}
	# lib1.a then lib2.o
	${WLD} main.o -L . -l1 -l2 -o main_l1_l2

run-main_l1_o2: main_l1_o2
	@MSG=$$(./main_l1_o2); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "main_l1_o2 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection main_l1_o2 test passed"

run-main_o1_l2: main_o1_l2
	@MSG=$$(./main_o1_l2); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "main_o1_l2 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection main_o1_l2 test passed"

run-main_l1_l2: main_l1_l2
	@MSG=$$(./main_l1_l2); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "main_l1_l2 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection main_l1_l2 test passed"

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f main_*


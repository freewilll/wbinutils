WLD = ../../../bin/wld

all: test

test: run-main12 run-mainl12 run-main21 run-mainl21

%.o: %.c
	gcc $< -c -o $@

%.o: %.s
	as $< -o $@

%.a: %.o
	ar rcs $@ $<

main12: main.o lib1.o lib2.o ${WLD}
	# lib1.o then lib2.o
	${WLD} main.o lib1.o lib2.o -o main12

mainl12: main.o lib1.a lib2.o ${WLD}
	# lib1.a then lib2.o
	${WLD} main.o -L . -l1 lib2.o -o mainl12

main21: main.o lib1.o lib2.o ${WLD}
	# lib2.o then lib1.o
	${WLD} main.o lib2.o lib1.o -o main21

mainl21: main.o lib1.o lib2.a ${WLD}
	# lib1.a then lib2.o
	${WLD} main.o lib1.o -L . -l2 -o mainl21

run-main12: main12
	@MSG=$$(./main12); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "main12 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection main12 test passed"

run-mainl12: mainl12
	@MSG=$$(./mainl12); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "mainl12 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection mainl12 test passed"

run-main21: main21
	@MSG=$$(./main21); ERR=$$? ;\
	if [ "$$ERR" != "2" ]; then \
		echo "main21 Exit code was $$ERR, not 2."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection main21 test passed"

run-mainl21: mainl21
	@MSG=$$(./mainl21); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "mainl21 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection mainl21 test passed"

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f main12
	@rm -f main21
	@rm -f mainl12
	@rm -f mainl21

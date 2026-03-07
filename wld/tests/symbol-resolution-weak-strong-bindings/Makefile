# Add three symbols of the same name:
# The first one is undefined in a main object file. It is either strong or weak.
# The second one can be a object or lib and weak or strong
# The third is like the second

# These tests also have much more concise unit tests,
# so are somewhat redundant. But they are left here anyways, since
# they are good e2e tests.
WLD ?= ../../../bin/wld

all: test

test: \
	run-mains_os_ow \
	run-mains_ls_ow \
	run-mains_os_lw \
	run-mains_ls_lw \
	run-mains_ow_os \
	run-mains_lw_os \
	run-mains_ow_ls \
	run-mains_lw_ls \
	run-mainw_os_ow \
	run-mainw_ls_ow \
	run-mainw_os_lw \
	run-mainw_ls_lw \
	run-mainw_ow_os \
	run-mainw_lw_os \
	run-mainw_ow_ls \
	run-mainw_lw_ls \

%.o: %.c
	gcc $< -c -o $@

%.o: %.s
	as $< -o $@

%.a: %.o
	ar rcs $@ $<

mains_os_ow: mains.o libs.o libw.o ${WLD}
	${WLD} mains.o libs.o libw.o -o mains_os_ow -static

mains_ls_ow: mains.o libs.a libw.o ${WLD}
	${WLD} mains.o -L . -ls libw.o -o mains_ls_ow -static

mains_os_lw: mains.o libs.o libw.a ${WLD}
	${WLD} mains.o libs.o -L . -lw -o mains_os_lw -static

mains_ls_lw: mains.o libs.a libw.a ${WLD}
	${WLD} mains.o -L . -ls -lw -o mains_ls_lw -static

mains_ow_os: mains.o libs.o libw.o ${WLD1}
	${WLD} mains.o libw.o libs.o -o mains_ow_os -static

mains_ow_ls: mains.o libs.a libw.o ${WLD}
	${WLD} mains.o libw.o -L . -ls -o mains_ow_ls -static

mains_lw_os: mains.o libs.o libw.a ${WLD}
	${WLD} mains.o -L . -lw libs.o -o mains_lw_os -static

mains_lw_ls: mains.o libs.a libw.a ${WLD}
	${WLD} mains.o -L . -lw -ls -o mains_lw_ls -static

mainw_os_ow: mainw.o libs.o libw.o ${WLD}
	${WLD} mainw.o libs.o libw.o -o mainw_os_ow -static

mainw_ls_ow: mainw.o libs.a libw.o ${WLD}
	${WLD} mainw.o -L . -ls libw.o -o mainw_ls_ow -static

mainw_os_lw: mainw.o libs.o libw.a ${WLD}
	${WLD} mainw.o libs.o -L . -lw -o mainw_os_lw -static

mainw_ls_lw: mainw.o libs.a libw.a ${WLD}
	${WLD} mainw.o -L . -ls -lw -o mainw_ls_lw -static

mainw_ow_os: mainw.o libs.o libw.o ${WLD1}
	${WLD} mainw.o libw.o libs.o -o mainw_ow_os -static

mainw_ow_ls: mainw.o libs.a libw.o ${WLD}
	${WLD} mainw.o libw.o -L . -ls -o mainw_ow_ls -static

mainw_lw_os: mainw.o libs.o libw.a ${WLD}
	${WLD} mainw.o -L . -lw libs.o -o mainw_lw_os -static

mainw_lw_ls: mainw.o libs.a libw.a ${WLD}
	${WLD} mainw.o -L . -lw -ls -o mainw_lw_ls -static

run-mains_%: mains_%
	@./$<; ERR=$$? ; if [ "$$ERR" != "1" ]; then echo "Exit code was $$ERR, not 1."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mains_%_ls: mains_%_ls
	@./$<; ERR=$$? ; if [ "$$ERR" != "2" ]; then echo "Exit code was $$ERR, not 2."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_%: mainw_%
	@./$<; ERR=$$? ; if [ "$$ERR" != "2" ]; then echo "Exit code was $$ERR, not 2."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_os_%: mainw_os_%
	@./$<; ERR=$$? ; if [ "$$ERR" != "1" ]; then echo "Exit code was $$ERR, not 1."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_ls_lw: mainw_ls_lw
	@./$<; ERR=$$? ; if [ "$$ERR" != "0" ]; then echo "Exit code was $$ERR, not 0."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_%_os: mainw_%_os
	@./$<; ERR=$$? ; if [ "$$ERR" != "1" ]; then echo "Exit code was $$ERR, not 1."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_lw_ls: mainw_lw_ls
	@./$<; ERR=$$? ; if [ "$$ERR" != "0" ]; then echo "Exit code was $$ERR, not 0."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

clean:
	@rm -f *.o
	@rm -f *.a
	@rm -f mains_*
	@rm -f mainw_*

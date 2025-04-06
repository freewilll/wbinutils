TARGET = ../bin/wld

all: ${TARGET}

HEADERS = \
	list.h \
	strmap.h \
	elf.h \
	list.h \
	ro-elf.h \
	rw-elf.h \
	strmap.h \
	error.h \
	wld/libs.h \
	wld/relocations.h \
	wld/symbols.h \
	wld/wld.h \

OBJECTS = \
	libs.o \
	relocations.o \
	symbols.o \
	wld.o \

LIBS = \
	elf \
	error \
	list \
	strmap \

HEADERS_PATH := ${HEADERS:%=../include/%}

LIBS_PREFIXED_L := ${LIBS:%=-l%}
LIBS_PATH := ${LIBS:%=../lib/lib%.a}

%.o: %.c ${HEADERS_PATH}
	gcc -g -Werror -I ../include -c $< -o $@

${TARGET}: ${OBJECTS} main.o ${LIBS_PATH}
	gcc -g -Werror -I ../include -L../lib ${OBJECTS} main.o ${LIBS_PREFIXED_L} -o ${TARGET}

test: ${TARGET} run-test-archive-loading run-test-symbol-resolution
	make -C tests

libtest-archive-loading.a: test-archive-loading.o
	ar rcs libtest-archive-loading.a test-archive-loading.o

libtest-archive-loading-long.a: test-archive-loading.o
	cp test-archive-loading.o /tmp/a-very-very-very-long-object-filename.o
	ar rcs libtest-archive-loading-long.a test-archive-loading.o /tmp/a-very-very-very-long-object-filename.o

test-archive-loading: ${OBJECTS} test-archive-loading.o
	ar rcs libtest-archive-loading.a test-archive-loading.o
	gcc -g -Werror -I ../include -L../lib ${OBJECTS} test-archive-loading.o ${LIBS_PREFIXED_L} -o test-archive-loading

.PHONY: run-test-archive-loading
run-test-archive-loading: test-archive-loading libtest-archive-loading.a libtest-archive-loading-long.a
	./test-archive-loading

test-symbol-resolution: ${OBJECTS} test-symbol-resolution.o
	gcc -g -Werror ${OBJECTS} test-symbol-resolution.o -L../lib ${LIBS_PREFIXED_L} -o test-symbol-resolution

.PHONY: run-test-symbol-resolution
run-test-symbol-resolution: test-symbol-resolution
	./test-symbol-resolution

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@make -C tests clean
	@rm -f test-archive-loading
	@rm -f libtest-archive-loading.a
	@rm -f libtest-archive-loading-long.a
	@rm -f test-symbol-resolution

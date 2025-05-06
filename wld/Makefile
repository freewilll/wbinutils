TARGET = ../bin/wld

all: ${TARGET}

HEADERS = \
	list.h \
	strmap.h \
	strmap-ordered.h \
	elf.h \
	list.h \
	ro-elf.h \
	rw-elf.h \
	error.h \
	wld/expr.h \
	wld/lexer.h \
	wld/libs.h \
	wld/parser.h \
	wld/relocations.h \
	wld/script.h \
	wld/symbols.h \
	wld/utils.h \
	wld/wld.h \

OBJECTS = \
	expr.o \
	lexer.o \
	libs.o \
	parser.o \
	relocations.o \
	script.o \
	symbols.o \
	utils.o \
	wld.o \

LIBS = \
	elf \
	error \
	list \
	strmap \
	strmap_ordered \

HEADERS_PATH := ${HEADERS:%=../include/%}

LIBS_PREFIXED_L := ${LIBS:%=-l%}
LIBS_PATH := ${LIBS:%=../lib/lib%.a}

%.o: %.c ${HEADERS_PATH}
	gcc -g -Werror -I ../include -c $< -o $@

libwld.a: ${OBJECTS} ${LIBS_PATH}
	ar rcsT libwld.a ${OBJECTS} ${LIBS_PATH}

${TARGET}: libwld.a main.o ${LIBS_PATH}
	gcc -g -Werror -I ../include main.o libwld.a -o ${TARGET}

test: ${TARGET} run-test-archive-loading run-test-symbol-resolution run-test-relocations run-test-parser run-test-expr run-test-utils
	make -C tests

libtest-archive-loading.a: test-archive-loading.o
	ar rcs libtest-archive-loading.a test-archive-loading.o

libtest-archive-loading-long.a: test-archive-loading.o
	cp test-archive-loading.o /tmp/a-very-very-very-long-object-filename.o
	ar rcs libtest-archive-loading-long.a test-archive-loading.o /tmp/a-very-very-very-long-object-filename.o

test-archive-loading: libwld.a test-archive-loading.o
	ar rcs libtest-archive-loading.a test-archive-loading.o
	gcc -g -Werror -I ../include test-archive-loading.o libwld.a  -o test-archive-loading

.PHONY: run-test-archive-loading
run-test-archive-loading: test-archive-loading libtest-archive-loading.a libtest-archive-loading-long.a
	./test-archive-loading

test-%: test-%.o libwld.a
	gcc -g -Werror $< libwld.a -o $@

.PHONY: run-test-symbol-resolution
run-test-symbol-resolution: test-symbol-resolution
	./test-symbol-resolution

.PHONY: run-test-relocations
run-test-relocations: test-relocations
	./test-relocations

.PHONY: run-test-parser
run-test-parser: test-parser
	./test-parser

.PHONY: run-test-expr
run-test-expr: test-expr
	./test-expr

.PHONY: run-test-utils
run-test-utils: test-utils
	./test-utils

clean:
	@rm -f *.o
	@rm -f libwld.a
	@rm -f ${TARGET}
	@make -C tests clean
	@rm -f test-archive-loading
	@rm -f libtest-archive-loading.a
	@rm -f libtest-archive-loading-long.a
	@rm -f test-symbol-resolution
	@rm -f test-relocations
	@rm -f test-parser
	@rm -f test-expr
	@rm -f test-utils

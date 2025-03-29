TARGET = ../bin/was

all: ${TARGET}

HEADERS = \
	error.h \
	list.h \
	strmap.h \
	was/base128.h \
	was/branches.h \
	was/dwarf.h \
	was/elf.h \
	was/expr.h \
	was/instr.h \
	was/lexer.h \
	was/list.h \
	was/opcodes.h \
	was/parser.h \
	was/relocations.h \
	was/strmap.h \
	was/symbols.h \
	was/utils.h \
	was/was.h \

OBJECTS = \
	base128.o \
	branches.o \
	dwarf.o \
	elf.o \
	expr.o \
	instr.o \
	lexer.o \
	opcodes.o \
	opcodes-generated.o \
	parser.o \
	relocations.o \
	symbols.o \
	utils.o \
	was.o \

LIBS = \
	error \
	list \
	strmap \

HEADERS_PATH := ${HEADERS:%=../include/%}

LIBS_PREFIXED_L := ${LIBS:%=-l%}
LIBS_PATH := ${LIBS:%=../lib/lib%.a}

%.o: %.c ${HEADERS_PATH}
	gcc -g -Werror -I ../include -c $< -o $@

scripts/venv:
	python3 -m virtualenv scripts/venv
	scripts/venv/bin/pip install bs4 lxml jinja2

# Requires a python virtualenv to be setup in scripts/venv
opcodes-generated.c: scripts/parse-x86reference.xml.py scripts/opcodes.j2 scripts/venv
	scripts/venv/bin/python3 scripts/parse-x86reference.xml.py ../../x86reference-2.xml opcodes-generated.c

${TARGET}: ${OBJECTS} main.o ${HEADERS_PATH}
	gcc -g -Werror -L../lib ${OBJECTS} ${LIBS_PREFIXED_L} main.o -o ${TARGET}

.PHONY: test
test: ${TARGET} run-test-instr run-test-data run-test-expr
	make -C tests

test-utils.o: test-utils.c ../include/was/test-utils.h
	gcc -g -Werror  -Wunused -I ../include -c $< -o $@

test-instr: ${OBJECTS} test-instr.o test-utils.o
	gcc -g -Werror -L../lib ${OBJECTS} test-instr.o test-utils.o ${LIBS_PREFIXED_L} -o test-instr

test-data: ${OBJECTS} test-data.o test-utils.o
	gcc -g -Werror -L../lib ${OBJECTS} test-data.o test-utils.o ${LIBS_PREFIXED_L} -o test-data

test-expr: ${OBJECTS} test-expr.o test-utils.o
	gcc -g -Werror -L../lib ${OBJECTS} test-expr.o test-utils.o ${LIBS_PREFIXED_L} -o test-expr

.PHONY: run-test-instr
run-test-instr: test-instr
	./test-instr

.PHONY: run-test-data
run-test-data: test-data
	./test-data

.PHONY: run-test-expr
run-test-expr: test-expr
	./test-expr

clean:
	@rm -f *.o
	@rm -f was
	@rm -f test-instr
	@rm -f test-expr
	@rm -f test-data
	@rm -Rf build/willos

	make -C tests clean

# Not done by default since it doesn't change that often
clean-venv:
	@rm -Rf scripts/venv

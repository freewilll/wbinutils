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
	wld/wld.h \

OBJECTS = \
	main.o \
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
	gcc -g -I ../include -c $< -o $@

${TARGET}: ${OBJECTS} ${LIBS_PATH}
	gcc -g -I ../include -L../lib ${OBJECTS} ${LIBS_PREFIXED_L} -o ${TARGET}

test: ${TARGET}
	make -C tests

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@make -C tests clean

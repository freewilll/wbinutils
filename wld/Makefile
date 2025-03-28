TARGET = ../bin/wld

all: ${TARGET}

HEADERS = \
	../include/elf.h \
	../include/list.h \
	../include/ro-elf.h \
	../include/rw-elf.h \
	../include/strmap.h \
	../include/error.h \
	../include/wld.h \

OBJECTS = \
	main.o \
	wld.o \

LIBS = \
	elf \
	error \
	list \
	strmap \

LIBS_PREFIXED_L := ${LIBS:%=-l%}
LIBS_PATH := ${LIBS:%=../lib/lib%.a}

%.o: %.c ${HEADERS}
	gcc -g -I ../include -c $< -o $@

${TARGET}: ${OBJECTS} ${LIBS_PATH}
	gcc -g -I ../include -L../lib ${OBJECTS} ${LIBS_PREFIXED_L} -o ${TARGET}

test: ${TARGET}
	make -C tests

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@make -C tests clean

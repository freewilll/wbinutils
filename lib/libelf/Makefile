TARGET = ../../lib/libelf.a

all: ${TARGET}

HEADERS = \
	../../include/elf.h \
	../../include/input-elf.h \
	../../include/output-elf.h

OBJECTS = \
	input-elf.o \
	output-elf.o

%.o: %.c ${HEADERS}
	gcc -g -Wunused -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

clean:
	@rm -f *.o
	@rm -f ${TARGET}

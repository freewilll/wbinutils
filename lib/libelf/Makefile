TARGET = ../../lib/libelf.a

all: ${TARGET}

HEADERS = \
	../../include/elf.h \
	../../include/ro-elf.h \
	../../include/rw-elf.h

OBJECTS = \
	ro-elf.o \
	rw-elf.o

%.o: %.c ${HEADERS}
	gcc -g  -Wunused -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

clean:
	@rm -f *.o
	@rm -f ${TARGET}

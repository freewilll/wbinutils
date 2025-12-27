TARGET = ../../lib/libelf.a

all: ${TARGET}

DEPFILES = ${OBJECTS:.o=.d}

HEADERS = \
	../../include/elf.h \
	../../include/input-elf.h \
	../../include/output-elf.h

OBJECTS = \
	input-elf.o \
	output-elf.o

CFLAGS = -g -Wunused -I ../../include -MMD -MP

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

clean:
	@rm -f *.o
	@rm -f *.d
	@rm -f ${TARGET}

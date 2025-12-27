TARGET = ../../lib/liberror.a

all: ${TARGET}

DEPFILES = ${OBJECTS:.o=.d}

HEADERS = \
	../../include/error.h \

OBJECTS = \
	error.o

CFLAGS = -g -Werror -I ../../include -MMD -MP

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

clean:
	@rm -f *.o
	@rm -f *.d
	@rm -f ${TARGET}

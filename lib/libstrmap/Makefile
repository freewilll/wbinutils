TARGET = ../../lib/libstrmap.a

all: ${TARGET}

DEPFILES = ${OBJECTS:.o=.d}

HEADERS = \
	../../include/strmap.h \

OBJECTS = \
	strmap.o

CFLAGS = -g -Werror -I ../../include -MMD -MP

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

test-strmap: test-strmap.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-strmap.o -o $@

.PHONY: test
test: test-strmap
	./test-strmap

clean:
	@rm -f *.o
	@rm -f *.d
	@rm -f ${TARGET}
	@rm -f test-strmap

TARGET = ../../lib/libstrmap_ordered.a

all: ${TARGET}

DEPFILES = ${OBJECTS:.o=.d}

HEADERS = \
	../../include/strmap-ordered.h \

OBJECTS = \
	strmap-ordered.o

CFLAGS = -g -Werror -I ../../include -MMD -MP

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

test-strmap-ordered: test-strmap-ordered.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-strmap-ordered.o -o $@

.PHONY: test
test: test-strmap-ordered
	./test-strmap-ordered

clean:
	@rm -f *.o
	@rm -f *.d
	@rm -f ${TARGET}
	@rm -f test-strmap-ordered

TARGET = ../../lib/libmap_ordered.a

all: ${TARGET}

DEPFILES = ${OBJECTS:.o=.d}

HEADERS = \
	../../include/map-ordered.h \

OBJECTS = \
	map-ordered.o

CFLAGS = -g -Werror -I ../../include -MMD -MP

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

test-map-ordered: test-map-ordered.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-map-ordered.o -o $@

.PHONY: test
test: test-map-ordered
	./test-map-ordered

clean:
	@rm -f *.o
	@rm -f *.d
	@rm -f ${TARGET}
	@rm -f test-map-ordered

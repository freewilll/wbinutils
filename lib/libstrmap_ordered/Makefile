TARGET = ../../lib/libstrmap_ordered.a

all: ${TARGET}

HEADERS = \
	../../include/strmap-ordered.h \

OBJECTS = \
	strmap-ordered.o

%.o: %.c ${HEADERS}
	gcc -g -Werror -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

test-strmap-ordered: test-strmap-ordered.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-strmap-ordered.o -o $@

.PHONY: test
test: test-strmap-ordered
	./test-strmap-ordered

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@rm -f test-strmap-ordered

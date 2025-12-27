TARGET = ../../lib/liblist.a

all: ${TARGET}

DEPFILES = ${OBJECTS:.o=.d}

HEADERS = \
	../../include/list.h \

OBJECTS = \
	list.o

CFLAGS = -g -Werror -I ../../include -MMD -MP

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

test-list: test-list.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-list.o -o $@

.PHONY: test
test: test-list
	./test-list

clean:
	@rm -f *.o
	@rm -f *.d
	@rm -f ${TARGET}
	@rm -f test-list

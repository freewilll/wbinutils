TARGET = ../../lib/liblist.a

all: ${TARGET}

HEADERS = \
	../../include/list.h \

OBJECTS = \
	list.o

%.o: %.c ${HEADERS}
	gcc -g -Werror -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

test-list: test-list.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-list.o -o $@

.PHONY: test
test: test-list
	./test-list

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@rm -f test-list

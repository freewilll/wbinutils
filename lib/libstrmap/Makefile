TARGET = ../../lib/libstrmap.a

all: ${TARGET}

HEADERS = \
	../../include/strmap.h \

OBJECTS = \
	strmap.o

%.o: %.c ${HEADERS}
	gcc -g -Werror -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

test-strmap: test-strmap.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-strmap.o -o $@

.PHONY: test
test: test-strmap
	./test-strmap

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@rm -f test-strmap

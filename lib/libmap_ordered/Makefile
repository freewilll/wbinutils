TARGET = ../../lib/libmap_ordered.a

all: ${TARGET}

HEADERS = \
	../../include/map-ordered.h \

OBJECTS = \
	map-ordered.o

%.o: %.c ${HEADERS}
	gcc -g -Werror -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

test-map-ordered: test-map-ordered.o ${OBJECTS}
	gcc -g -Werror -I ../../include ${OBJECTS} test-map-ordered.o -o $@

.PHONY: test
test: test-map-ordered
	./test-map-ordered

clean:
	@rm -f *.o
	@rm -f ${TARGET}
	@rm -f test-map-ordered

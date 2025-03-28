TARGET = ../../lib/liblist.a

all: ${TARGET}

HEADERS = \
	../../include/list.h \

OBJECTS = \
	list.o

%.o: %.c ${HEADERS}
	gcc -g  -Wunused -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

clean:
	@rm -f *.o
	@rm -f ${TARGET}

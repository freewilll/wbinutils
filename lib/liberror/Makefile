TARGET = ../../lib/liberror.a

all: ${TARGET}

HEADERS = \
	../../include/error.h \

OBJECTS = \
	error.o

%.o: %.c ${HEADERS}
	gcc -g  -Wunused -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

clean:
	@rm -f *.o
	@rm -f ${TARGET}

TARGET = ../../lib/libstrmap.a

all: ${TARGET}

HEADERS = \
	../../include/strmap.h \

OBJECTS = \
	strmap.o

%.o: %.c ${HEADERS}
	gcc -g -Werror -I ../../include -c $< -o $@

${TARGET}: ${OBJECTS} ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

clean:
	@rm -f *.o
	@rm -f ${TARGET}

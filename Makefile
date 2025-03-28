LIBS = \
	lib/libelf.a \
	lib/liblist.a \
	lib/libstrmap.a \
	lib/liberror.a \

all: ${LIBS} was/was bin/wld

.PHONY: lib/liblist.a
lib/liblist.a:
	@make -C lib/liblist

.PHONY: lib/libstrmap.a
lib/libstrmap.a:
	@make -C lib/libstrmap

.PHONY: lib/liberror.a
lib/liberror.a:
	@make -C lib/liberror

.PHONY: lib/libelf.a
lib/libelf.a: lib/liblist.a lib/libstrmap.a
	@make -C lib/libelf

was/was: ${LIBS}
	@make -C was

bin/wld: ${LIBS}
	@make -C wld

clean:
	@make -C lib/liblist clean
	@make -C lib/libstrmap clean
	@make -C lib/liberror clean
	@make -C lib/libelf clean
	@make -C was clean
	@make -C wld clean

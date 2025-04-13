LIBS = \
	lib/libelf.a \
	lib/liblist.a \
	lib/libstrmap.a \
	lib/libstrmap_ordered.a \
	lib/liberror.a \

all: ${LIBS} bin/was bin/wld

.PHONY: lib/liblist.a
lib/liblist.a:
	@make -C lib/liblist

.PHONY: lib/libstrmap.a
lib/libstrmap.a:
	@make -C lib/libstrmap

.PHONY: lib/libstrmap_ordered.a
lib/libstrmap_ordered.a:
	@make -C lib/libstrmap_ordered

.PHONY: lib/liberror.a
lib/liberror.a:
	@make -C lib/liberror

.PHONY: lib/libelf.a
lib/libelf.a: lib/liblist.a lib/libstrmap.a
	@make -C lib/libelf

bin/was: ${LIBS}
	@make -C was

bin/wld: ${LIBS}
	@make -C wld

.PHONY: test
test: bin/was bin/wld lib/liblist.a lib/libstrmap.a lib/libstrmap_ordered.a
	@make -C was test
	@make -C wld test
	@make -C lib/liblist test
	@make -C lib/libstrmap test
	@make -C lib/libstrmap_ordered test

clean:
	@make -C lib/liblist clean
	@make -C lib/libstrmap clean
	@make -C lib/libstrmap_ordered clean
	@make -C lib/liberror clean
	@make -C lib/libelf clean
	@make -C was clean
	@make -C wld clean

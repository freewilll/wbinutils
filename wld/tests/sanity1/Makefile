WLD = ../../../bin/wld

all: test

test: run-sanity1 test-sanity1-symbol-table

%.o: %.s
	as $< -o $@

sanity1: sanity1.o ${WLD}
	${WLD} sanity1.o -o sanity1

run-sanity1: sanity1
	@./sanity1; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Sanity 1 test passed"; \
	fi

test-sanity1-symbol-table: sanity1
	@# Crude test that the symbol table is ok

	@SYMTAB=$$(readelf -s sanity1) ;\
	echo $$SYMTAB | grep -v corrupt >/dev/null && \
	echo $$SYMTAB | grep " 0: 0000000000000000 0 NOTYPE LOCAL DEFAULT UND"                        > /dev/null && \
	echo $$SYMTAB | grep " 1: 0000000000000000 0 SECTION LOCAL DEFAULT 1"                         > /dev/null && \
	echo $$SYMTAB | grep " 2: 0000000000000000 0 SECTION LOCAL DEFAULT 2"                         > /dev/null && \
	echo $$SYMTAB | grep " 3: 0000000000000000 0 SECTION LOCAL DEFAULT 3"                         > /dev/null && \
	echo $$SYMTAB | grep " 4: 0000000000000000 0 SECTION LOCAL DEFAULT 4"                         > /dev/null && \
	echo $$SYMTAB | grep " 5: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS _GLOBAL_OFFSET_TABLE_"  > /dev/null && \
	echo $$SYMTAB | grep " 6: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS __preinit_array_start"  > /dev/null && \
	echo $$SYMTAB | grep " 7: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS __preinit_array_end"    > /dev/null && \
	echo $$SYMTAB | grep " 8: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS __init_array_start"     > /dev/null && \
	echo $$SYMTAB | grep " 9: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS __init_array_end"       > /dev/null && \
	echo $$SYMTAB | grep "10: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS __fini_array_start"     > /dev/null && \
	echo $$SYMTAB | grep "11: 0000000000000000 0 NOTYPE GLOBAL HIDDEN ABS __fini_array_end"       > /dev/null && \
	echo $$SYMTAB | grep "12: 0000000000401000 0 NOTYPE GLOBAL DEFAULT 1 _start"                  > /dev/null && \
	echo $$SYMTAB | grep "13: 000000000040100c 0 NOTYPE GLOBAL DEFAULT ABS __etext"               > /dev/null && \
	echo $$SYMTAB | grep "14: 000000000040100c 0 NOTYPE GLOBAL DEFAULT ABS _etext"                > /dev/null && \
	echo $$SYMTAB | grep "15: 000000000040100c 0 NOTYPE GLOBAL DEFAULT ABS etext"                 > /dev/null

clean:
	@rm -f *.o
	@rm -f sanity1

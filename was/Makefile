LOCAL_BUILD_DIR := $(BUILD_DIR)/was
LOCAL_SRC_DIR := $(SRC_DIR)/was

TARGET = $(BUILD_DIR)/bin/was

all: ${TARGET}

HEADERS = \
	elf.h \
	error.h \
	list.h \
	output-elf.h \
	strmap.h \
	strmap-ordered.h \
	was/base128.h \
	was/branches.h \
	was/dwarf.h \
	was/elf.h \
	was/expr.h \
	was/instr.h \
	was/lexer.h \
	was/opcodes.h \
	was/parser.h \
	was/relocations.h \
	was/symbols.h \
	was/test-utils.h \
	was/utils.h \
	was/was.h \

OBJECTS = \
	base128.o \
	branches.o \
	dwarf.o \
	elf.o \
	expr.o \
	instr.o \
	lexer.o \
	opcodes.o \
	opcodes-generated.o \
	parser.o \
	relocations.o \
	symbols.o \
	utils.o \
	was.o \

LIBS = \
	elf \
	error \
	list \
	strmap \
	strmap_ordered \

HEADERS_PATH := ${HEADERS:%=$(SRC_DIR)/include/%}
LIBS_PATH := ${LIBS:%=$(BUILD_DIR)/lib/lib%.a}

OBJECTS := $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS))

DEPFILES = \
	${OBJECTS:.o=.d} \
	$(LOCAL_BUILD_DIR)/main.d \
	$(LOCAL_BUILD_DIR)/test-utils.d \
	$(LOCAL_BUILD_DIR)/test-instr.d \
	$(LOCAL_BUILD_DIR)/test-data.d \
	$(LOCAL_BUILD_DIR)/test-expr.d

CFLAGS = -g -Werror -I $(SRC_DIR)/include -MMD -MP

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c ${HEADERS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc ${CFLAGS} -c $< -o $@

$(LOCAL_BUILD_DIR)/scripts/venv:
	python3 -m virtualenv $@
	$@/bin/pip install -r $(LOCAL_SRC_DIR)/scripts/requirements.txt

# Keep generated files in the build tree when regeneration is requested.
$(LOCAL_BUILD_DIR)/opcodes-generated.c: $(LOCAL_SRC_DIR)/scripts/parse-x86reference.xml.py $(LOCAL_SRC_DIR)/scripts/opcodes.j2 $(LOCAL_BUILD_DIR)/scripts/venv
	mkdir -p $(LOCAL_BUILD_DIR)
	$(LOCAL_BUILD_DIR)/scripts/venv/bin/python3 $(LOCAL_SRC_DIR)/scripts/parse-x86reference.xml.py $(SRC_DIR)/x86reference-2.xml $@

${TARGET}: ${OBJECTS} $(LOCAL_BUILD_DIR)/main.o ${HEADERS_PATH} ${LIBS_PATH}
	mkdir -p $(dir $@)
	gcc -g -Werror ${OBJECTS} ${LIBS_PATH} $(LOCAL_BUILD_DIR)/main.o -o ${TARGET}

-include ${DEPFILES}

.PHONY: test
test: ${TARGET} run-test-instr run-test-data run-test-expr
	+$(MAKE) -C $(LOCAL_SRC_DIR)/tests test
	@echo was tests passed

$(LOCAL_BUILD_DIR)/test-instr: ${OBJECTS} $(LOCAL_BUILD_DIR)/test-instr.o $(LOCAL_BUILD_DIR)/test-utils.o ${LIBS_PATH}
	gcc -g -Werror ${OBJECTS} $(LOCAL_BUILD_DIR)/test-instr.o $(LOCAL_BUILD_DIR)/test-utils.o ${LIBS_PATH} -o $@

$(LOCAL_BUILD_DIR)/test-data: ${OBJECTS} $(LOCAL_BUILD_DIR)/test-data.o $(LOCAL_BUILD_DIR)/test-utils.o ${LIBS_PATH}
	gcc -g -Werror ${OBJECTS} $(LOCAL_BUILD_DIR)/test-data.o $(LOCAL_BUILD_DIR)/test-utils.o ${LIBS_PATH} -o $@

$(LOCAL_BUILD_DIR)/test-expr: ${OBJECTS} $(LOCAL_BUILD_DIR)/test-expr.o $(LOCAL_BUILD_DIR)/test-utils.o ${LIBS_PATH}
	gcc -g -Werror ${OBJECTS} $(LOCAL_BUILD_DIR)/test-expr.o $(LOCAL_BUILD_DIR)/test-utils.o ${LIBS_PATH} -o $@

.PHONY: run-test-instr
run-test-instr: $(LOCAL_BUILD_DIR)/test-instr
	$<

.PHONY: run-test-data
run-test-data: $(LOCAL_BUILD_DIR)/test-data
	$<

.PHONY: run-test-expr
run-test-expr: $(LOCAL_BUILD_DIR)/test-expr
	$<

clean:
	@rm -f $(OBJECTS)
	@rm -f ${OBJECTS:.o=.d}
	@rm -f $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/main.d
	@rm -f $(LOCAL_BUILD_DIR)/test-utils.o $(LOCAL_BUILD_DIR)/test-utils.d
	@rm -f $(LOCAL_BUILD_DIR)/test-instr.o $(LOCAL_BUILD_DIR)/test-instr.d
	@rm -f $(LOCAL_BUILD_DIR)/test-data.o $(LOCAL_BUILD_DIR)/test-data.d
	@rm -f $(LOCAL_BUILD_DIR)/test-expr.o $(LOCAL_BUILD_DIR)/test-expr.d
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/test-instr
	@rm -f $(LOCAL_BUILD_DIR)/test-expr
	@rm -f $(LOCAL_BUILD_DIR)/test-data
	@rm -Rf $(LOCAL_BUILD_DIR)/build/willos
	@+$(MAKE) -C $(LOCAL_SRC_DIR)/tests clean

# Not done by default since it doesn't change that often
clean-venv:
	@rm -Rf $(LOCAL_BUILD_DIR)/scripts/venv

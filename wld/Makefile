LOCAL_BUILD_DIR := $(BUILD_DIR)/wld
LOCAL_SRC_DIR := $(SRC_DIR)/wld

TARGET = $(BUILD_DIR)/bin/wld

all: ${TARGET}

HEADERS = \
	list.h \
	map-ordered.h \
	strmap.h \
	strmap-ordered.h \
	elf.h \
	list.h \
	input-elf.h \
	output-elf.h \
	error.h \
	wld/expr.h \
	wld/layout.h \
	wld/lexer.h \
	wld/libs.h \
	wld/parser.h \
	wld/relocations.h \
	wld/script.h \
	wld/symbols.h \
	wld/utils.h \
	wld/wld.h \

OBJECTS = \
	expr.o \
	layout.o \
	lexer.o \
	libs.o \
	parser.o \
	relocations.o \
	script.o \
	symbols.o \
	utils.o \
	wld.o \

LIBS = \
	elf \
	error \
	list \
	map_ordered \
	strmap \
	strmap_ordered \

HEADERS_PATH := ${HEADERS:%=$(SRC_DIR)/include/%}
LIBS_PATH := ${LIBS:%=$(BUILD_DIR)/lib/lib%.a}

OBJECTS := $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS))

LINKER_SCRIPTS_HEADERS = \
	$(LOCAL_BUILD_DIR)/default.ld.h \
	$(LOCAL_BUILD_DIR)/default-shared.ld.h \

DEPFILES = \
	${OBJECTS:.o=.d} \
	$(LOCAL_BUILD_DIR)/main.d \
	$(LOCAL_BUILD_DIR)/test-archive-loading.d \
	$(LOCAL_BUILD_DIR)/test-symbol-resolution.d \
	$(LOCAL_BUILD_DIR)/test-relocations.d \
	$(LOCAL_BUILD_DIR)/test-parser.d \
	$(LOCAL_BUILD_DIR)/test-expr.d \
	$(LOCAL_BUILD_DIR)/test-utils.d \
	$(LOCAL_BUILD_DIR)/test-e2e.d

CFLAGS = -g -Werror -I $(LOCAL_BUILD_DIR) -I $(LOCAL_SRC_DIR) -I $(SRC_DIR)/include -MMD -MP

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c ${HEADERS_PATH} ${LINKER_SCRIPTS_HEADERS}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc ${CFLAGS} -c $< -o $@

$(LOCAL_BUILD_DIR)/default.ld.h: $(LOCAL_SRC_DIR)/default-linker-script.template.h
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -E $< -D STATIC > $@

$(LOCAL_BUILD_DIR)/default-shared.ld.h: $(LOCAL_SRC_DIR)/default-linker-script.template.h
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -E $< -D SHARED > $@

$(LOCAL_BUILD_DIR)/libwld.a: ${OBJECTS} ${LIBS_PATH} ${LINKER_SCRIPTS_HEADERS}
	mkdir -p $(LOCAL_BUILD_DIR)
	ar rcs $@ ${OBJECTS}

${TARGET}: $(LOCAL_BUILD_DIR)/libwld.a $(LOCAL_BUILD_DIR)/main.o ${LIBS_PATH}
	mkdir -p $(dir $@)
	gcc -g -Werror -I $(LOCAL_BUILD_DIR) -I $(LOCAL_SRC_DIR) -I $(SRC_DIR)/include $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o ${TARGET}

-include ${DEPFILES}

.PHONY: test
test: ${TARGET} run-test-archive-loading run-test-symbol-resolution run-test-relocations run-test-parser run-test-expr run-test-utils run-test-e2e
	+$(MAKE) -C $(LOCAL_SRC_DIR)/tests test

$(LOCAL_BUILD_DIR)/libtest-archive-loading.a: $(LOCAL_BUILD_DIR)/test-archive-loading.o
	mkdir -p $(LOCAL_BUILD_DIR)
	ar rcs $@ $<

$(LOCAL_BUILD_DIR)/libtest-archive-loading-long.a: $(LOCAL_BUILD_DIR)/test-archive-loading.o
	mkdir -p $(LOCAL_BUILD_DIR)
	cp $< $(LOCAL_BUILD_DIR)/a-very-very-very-long-object-filename.o
	ar rcs $@ $< $(LOCAL_BUILD_DIR)/a-very-very-very-long-object-filename.o

$(LOCAL_BUILD_DIR)/test-archive-loading: $(LOCAL_BUILD_DIR)/libwld.a $(LOCAL_BUILD_DIR)/test-archive-loading.o $(LOCAL_BUILD_DIR)/libtest-archive-loading-long.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror -I $(LOCAL_BUILD_DIR) -I $(LOCAL_SRC_DIR) -I $(SRC_DIR)/include $(LOCAL_BUILD_DIR)/test-archive-loading.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-archive-loading
run-test-archive-loading: $(LOCAL_BUILD_DIR)/test-archive-loading $(LOCAL_BUILD_DIR)/libtest-archive-loading.a $(LOCAL_BUILD_DIR)/libtest-archive-loading-long.a
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

$(LOCAL_BUILD_DIR)/test-symbol-resolution: $(LOCAL_BUILD_DIR)/test-symbol-resolution.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror $< $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-symbol-resolution
run-test-symbol-resolution: $(LOCAL_BUILD_DIR)/test-symbol-resolution
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

$(LOCAL_BUILD_DIR)/test-relocations: $(LOCAL_BUILD_DIR)/test-relocations.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror $< $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-relocations
run-test-relocations: $(LOCAL_BUILD_DIR)/test-relocations
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

$(LOCAL_BUILD_DIR)/test-parser: $(LOCAL_BUILD_DIR)/test-parser.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror $< $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-parser
run-test-parser: $(LOCAL_BUILD_DIR)/test-parser
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

$(LOCAL_BUILD_DIR)/test-expr: $(LOCAL_BUILD_DIR)/test-expr.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror $< $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-expr
run-test-expr: $(LOCAL_BUILD_DIR)/test-expr
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

$(LOCAL_BUILD_DIR)/test-utils: $(LOCAL_BUILD_DIR)/test-utils.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror $< $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-utils
run-test-utils: $(LOCAL_BUILD_DIR)/test-utils
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

$(LOCAL_BUILD_DIR)/test-e2e: $(LOCAL_BUILD_DIR)/test-e2e.o $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc -g -Werror $< $(LOCAL_BUILD_DIR)/libwld.a ${LIBS_PATH} -o $@

.PHONY: run-test-e2e
run-test-e2e: $(LOCAL_BUILD_DIR)/test-e2e
	cd $(LOCAL_BUILD_DIR) && ./$(notdir $<)

clean:
	@rm -f $(OBJECTS)
	@rm -f ${OBJECTS:.o=.d}
	@rm -f $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/main.d
	@rm -f $(LOCAL_BUILD_DIR)/test-archive-loading.o $(LOCAL_BUILD_DIR)/test-archive-loading.d
	@rm -f $(LOCAL_BUILD_DIR)/test-symbol-resolution.o $(LOCAL_BUILD_DIR)/test-symbol-resolution.d
	@rm -f $(LOCAL_BUILD_DIR)/test-relocations.o $(LOCAL_BUILD_DIR)/test-relocations.d
	@rm -f $(LOCAL_BUILD_DIR)/test-parser.o $(LOCAL_BUILD_DIR)/test-parser.d
	@rm -f $(LOCAL_BUILD_DIR)/test-expr.o $(LOCAL_BUILD_DIR)/test-expr.d
	@rm -f $(LOCAL_BUILD_DIR)/test-utils.o $(LOCAL_BUILD_DIR)/test-utils.d
	@rm -f $(LOCAL_BUILD_DIR)/test-e2e.o $(LOCAL_BUILD_DIR)/test-e2e.d
	@rm -f $(LINKER_SCRIPTS_HEADERS)
	@rm -f $(LOCAL_BUILD_DIR)/libwld.a
	@rm -f ${TARGET}
	@rm -f $(LOCAL_BUILD_DIR)/test-archive-loading
	@rm -f $(LOCAL_BUILD_DIR)/libtest-archive-loading.a
	@rm -f $(LOCAL_BUILD_DIR)/libtest-archive-loading-long.a
	@rm -f $(LOCAL_BUILD_DIR)/a-very-very-very-long-object-filename.o
	@rm -f $(LOCAL_BUILD_DIR)/test-symbol-resolution
	@rm -f $(LOCAL_BUILD_DIR)/test-relocations
	@rm -f $(LOCAL_BUILD_DIR)/test-parser
	@rm -f $(LOCAL_BUILD_DIR)/test-expr
	@rm -f $(LOCAL_BUILD_DIR)/test-utils
	@rm -f $(LOCAL_BUILD_DIR)/test-e2e
	@+$(MAKE) -C $(LOCAL_SRC_DIR)/tests clean

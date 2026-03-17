LOCAL_BUILD_DIR := $(BUILD_DIR)/lib/libstrmap_ordered
LOCAL_SRC_DIR := $(SRC_DIR)/lib/libstrmap_ordered

TARGET = $(BUILD_DIR)/lib/libstrmap_ordered.a

all: ${TARGET}

HEADERS = \
	$(SRC_DIR)/include/strmap-ordered.h \

OBJECTS = \
	strmap-ordered.o

DEPFILES = $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS:.o=.d))

OBJECTS := $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS))

CFLAGS = -g -Werror -I $(SRC_DIR)/include -MMD -MP

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c ${HEADERS}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	mkdir -p $(dir $@)
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

$(LOCAL_BUILD_DIR)/test-strmap-ordered: $(LOCAL_BUILD_DIR)/test-strmap-ordered.o ${OBJECTS}
	gcc -g -Werror -I $(SRC_DIR)/include ${OBJECTS} $(LOCAL_BUILD_DIR)/test-strmap-ordered.o -o $@

.PHONY: test
test: $(LOCAL_BUILD_DIR)/test-strmap-ordered
	$<

clean:
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.d
	@rm -f $(LOCAL_BUILD_DIR)/test-strmap-ordered

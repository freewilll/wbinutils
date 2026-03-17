LOCAL_BUILD_DIR := $(BUILD_DIR)/lib/libstrmap
LOCAL_SRC_DIR := $(SRC_DIR)/lib/libstrmap

TARGET = $(BUILD_DIR)/lib/libstrmap.a

all: ${TARGET}

DEPFILES = $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS:.o=.d))

HEADERS = \
	$(SRC_DIR)/include/strmap.h \

OBJECTS = \
	strmap.o

OBJECTS := $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS))

CFLAGS = -g -Werror -I $(SRC_DIR)/include -MMD -MP

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c ${HEADERS}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	mkdir -p $(dir $@)
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

$(LOCAL_BUILD_DIR)/test-strmap: $(LOCAL_BUILD_DIR)/test-strmap.o ${OBJECTS}
	gcc -g -Werror -I $(SRC_DIR)/include ${OBJECTS} $(LOCAL_BUILD_DIR)/test-strmap.o -o $@

.PHONY: test
test: $(LOCAL_BUILD_DIR)/test-strmap
	$<

clean:
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.d
	@rm -f $(LOCAL_BUILD_DIR)/test-strmap

LOCAL_BUILD_DIR := $(BUILD_DIR)/lib/liblist
LOCAL_SRC_DIR := $(SRC_DIR)/lib/liblist

TARGET = $(BUILD_DIR)/lib/liblist.a

all: ${TARGET}

DEPFILES = $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS:.o=.d))

HEADERS = \
	$(SRC_DIR)/include/list.h \

OBJECTS = \
	list.o

OBJECTS := $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS))

CFLAGS = -g -Werror -I $(SRC_DIR)/include -MMD -MP

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c ${HEADERS}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	mkdir -p $(dir $@)
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

$(LOCAL_BUILD_DIR)/test-list: $(LOCAL_BUILD_DIR)/test-list.o ${OBJECTS}
	gcc -g -Werror -I $(SRC_DIR)/include ${OBJECTS} $(LOCAL_BUILD_DIR)/test-list.o -o $@

.PHONY: test
test: $(LOCAL_BUILD_DIR)/test-list
	$<

clean:
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.d
	@rm -f $(LOCAL_BUILD_DIR)/test-list

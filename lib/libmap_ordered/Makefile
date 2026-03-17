LOCAL_BUILD_DIR := $(BUILD_DIR)/lib/libmap_ordered
LOCAL_SRC_DIR := $(SRC_DIR)/lib/libmap_ordered

TARGET = $(BUILD_DIR)/lib/libmap_ordered.a

all: ${TARGET}

HEADERS = \
	$(SRC_DIR)/include/map-ordered.h \

OBJECTS = \
	map-ordered.o

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

$(LOCAL_BUILD_DIR)/test-map-ordered: $(LOCAL_BUILD_DIR)/test-map-ordered.o ${OBJECTS}
	gcc -g -Werror -I $(SRC_DIR)/include ${OBJECTS} $(LOCAL_BUILD_DIR)/test-map-ordered.o -o $@

.PHONY: test
test: $(LOCAL_BUILD_DIR)/test-map-ordered
	$<

clean:
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.d
	@rm -f $(LOCAL_BUILD_DIR)/test-map-ordered

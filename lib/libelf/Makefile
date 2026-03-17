LOCAL_BUILD_DIR := $(BUILD_DIR)/lib/libelf
LOCAL_SRC_DIR := $(SRC_DIR)/lib/libelf

TARGET = $(BUILD_DIR)/lib/libelf.a

all: ${TARGET}

HEADERS = \
	$(SRC_DIR)/include/elf.h \
	$(SRC_DIR)/include/input-elf.h \
	$(SRC_DIR)/include/output-elf.h

OBJECTS = \
	input-elf.o \
	output-elf.o

DEPFILES = $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS:.o=.d))

OBJECTS := $(addprefix $(LOCAL_BUILD_DIR)/, $(OBJECTS))

CFLAGS = -g -Wunused -I $(SRC_DIR)/include -MMD -MP

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c ${HEADERS}
	mkdir -p $(LOCAL_BUILD_DIR)
	gcc ${CFLAGS} -c $< -o $@

${TARGET}: ${OBJECTS}
	mkdir -p $(dir $@)
	ar rcs ${TARGET} ${OBJECTS}

-include ${DEPFILES}

clean:
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.d

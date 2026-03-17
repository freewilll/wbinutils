LOCAL_BUILD_DIR := $(BUILD_DIR)/lib/liberror
LOCAL_SRC_DIR := $(SRC_DIR)/lib/liberror

TARGET = $(BUILD_DIR)/lib/liberror.a

all: ${TARGET}

HEADERS = \
	$(SRC_DIR)/include/error.h \

OBJECTS = \
	error.o

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

clean:
	@rm -f $(TARGET)
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.d

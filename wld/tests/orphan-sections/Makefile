$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/orphan-sections
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/orphan-sections

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-orphans

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/orphans: $(LOCAL_BUILD_DIR)/orphans.o $(LOCAL_BUILD_DIR)/orphans2.o ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/orphans.o $(LOCAL_BUILD_DIR)/orphans2.o -o $(LOCAL_BUILD_DIR)/orphans -static

run-orphans: $(LOCAL_BUILD_DIR)/orphans
	$(LOCAL_BUILD_DIR)/orphans

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/orphans

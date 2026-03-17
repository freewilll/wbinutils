$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-bindings
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-bindings

# These tests also have much more concise unit tests,
# so are redundant. But they are left here anyways, since
# they are good e2e tests.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main12 run-mainl12 run-main21 run-mainl21

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	${GCC} $< -c -o $@

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${WAS} $< -o $@

$(LOCAL_BUILD_DIR)/%.a: $(LOCAL_BUILD_DIR)/%.o | $(LOCAL_BUILD_DIR)
	ar rcs $@ $<

$(LOCAL_BUILD_DIR)/main12: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o ${WLD}
	# $(LOCAL_BUILD_DIR)/lib1.o then $(LOCAL_BUILD_DIR)/lib2.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o -o $(LOCAL_BUILD_DIR)/main12 -static

$(LOCAL_BUILD_DIR)/mainl12: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.a $(LOCAL_BUILD_DIR)/lib2.o ${WLD}
	# $(LOCAL_BUILD_DIR)/lib1.a then $(LOCAL_BUILD_DIR)/lib2.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L $(LOCAL_BUILD_DIR) -l1 $(LOCAL_BUILD_DIR)/lib2.o -o $(LOCAL_BUILD_DIR)/mainl12 -static

$(LOCAL_BUILD_DIR)/main21: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o ${WLD}
	# $(LOCAL_BUILD_DIR)/lib2.o then $(LOCAL_BUILD_DIR)/lib1.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib2.o $(LOCAL_BUILD_DIR)/lib1.o -o $(LOCAL_BUILD_DIR)/main21 -static

$(LOCAL_BUILD_DIR)/mainl21: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.a ${WLD}
	# $(LOCAL_BUILD_DIR)/lib1.a then $(LOCAL_BUILD_DIR)/lib2.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o -L $(LOCAL_BUILD_DIR) -l2 -o $(LOCAL_BUILD_DIR)/mainl21 -static

run-main12: $(LOCAL_BUILD_DIR)/main12
	@MSG=$$(./main12); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "$(LOCAL_BUILD_DIR)/main12 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/main12 test passed"

run-mainl12: $(LOCAL_BUILD_DIR)/mainl12
	@MSG=$$(./mainl12); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "$(LOCAL_BUILD_DIR)/mainl12 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/mainl12 test passed"

run-main21: $(LOCAL_BUILD_DIR)/main21
	@MSG=$$(./main21); ERR=$$? ;\
	if [ "$$ERR" != "2" ]; then \
		echo "$(LOCAL_BUILD_DIR)/main21 Exit code was $$ERR, not 2."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/main21 test passed"

run-mainl21: $(LOCAL_BUILD_DIR)/mainl21
	@MSG=$$(./mainl21); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "$(LOCAL_BUILD_DIR)/mainl21 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/mainl21 test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/main12
	@rm -f $(LOCAL_BUILD_DIR)/main21
	@rm -f $(LOCAL_BUILD_DIR)/mainl12
	@rm -f $(LOCAL_BUILD_DIR)/mainl21

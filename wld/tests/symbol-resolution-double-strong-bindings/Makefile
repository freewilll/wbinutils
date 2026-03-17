$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-double-strong-bindings
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-double-strong-bindings

# These tests also have much more concise unit tests,
# so are redundant. But they are left here anyways, since
# they are good e2e tests.

all: | $(LOCAL_BUILD_DIR)
all: test

test: $(LOCAL_BUILD_DIR)/main_o1_o2 $(LOCAL_BUILD_DIR)/main_l1_o2 run-main_o1_l2 run-main_l1_l2

# Test two strong symbols
# obj1 obj2 fail
# lib1 obj2 fail
# obj1 lib2 ok
# lib1 lib2 ok

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	${GCC} $< -c -o $@

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${WAS} $< -o $@

$(LOCAL_BUILD_DIR)/%.a: $(LOCAL_BUILD_DIR)/%.o | $(LOCAL_BUILD_DIR)
	ar rcs $@ $<

$(LOCAL_BUILD_DIR)/main_o1_o2: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o
	@MSG=$$(${WLD} $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o -o $(LOCAL_BUILD_DIR)/main_o1_o2 -static 2>/dev/null); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "wld exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection wld exit code for $(LOCAL_BUILD_DIR)/main_o1_o2 test passed"

$(LOCAL_BUILD_DIR)/main_l1_o2: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.a $(LOCAL_BUILD_DIR)/lib2.o
	@MSG=$$(${WLD} $(LOCAL_BUILD_DIR)/main.o -L $(LOCAL_BUILD_DIR) -l1 $(LOCAL_BUILD_DIR)/lib2.o -o $(LOCAL_BUILD_DIR)/main_l1_o2 -static 2>/dev/null); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "wld exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection wld exit code for $(LOCAL_BUILD_DIR)/main_l1_o2 test passed"

$(LOCAL_BUILD_DIR)/main_o1_l2: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.a
	# $(LOCAL_BUILD_DIR)/lib1.a then $(LOCAL_BUILD_DIR)/lib2.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o -L $(LOCAL_BUILD_DIR) -l2 -o $(LOCAL_BUILD_DIR)/main_o1_l2 -static

$(LOCAL_BUILD_DIR)/main_l1_l2: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.a $(LOCAL_BUILD_DIR)/lib2.a
	# $(LOCAL_BUILD_DIR)/lib1.a then $(LOCAL_BUILD_DIR)/lib2.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L $(LOCAL_BUILD_DIR) -l1 -l2 -o $(LOCAL_BUILD_DIR)/main_l1_l2 -static

run-main_l1_o2: $(LOCAL_BUILD_DIR)/main_l1_o2
	@MSG=$$(./main_l1_o2); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "$(LOCAL_BUILD_DIR)/main_l1_o2 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/main_l1_o2 test passed"

run-main_o1_l2: $(LOCAL_BUILD_DIR)/main_o1_l2
	@MSG=$$(./main_o1_l2); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "$(LOCAL_BUILD_DIR)/main_o1_l2 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/main_o1_l2 test passed"

run-main_l1_l2: $(LOCAL_BUILD_DIR)/main_l1_l2
	@MSG=$$(./main_l1_l2); ERR=$$? ;\
	if [ "$$ERR" != "1" ]; then \
		echo "$(LOCAL_BUILD_DIR)/main_l1_l2 Exit code was $$ERR, not 1."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection $(LOCAL_BUILD_DIR)/main_l1_l2 test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/main_*


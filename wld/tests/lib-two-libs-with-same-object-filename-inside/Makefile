$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/lib-two-libs-with-same-object-filename-inside
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/lib-two-libs-with-same-object-filename-inside

# Test a regression related to two static libs that have the same filename in it, with local symbols
# The looking for local symbol tables was based on filename alone instead of a combination of libname and filename.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} -o $@ $<

$(LOCAL_BUILD_DIR)/lib1dir:
	mkdir -p $(LOCAL_BUILD_DIR)/lib1dir

$(LOCAL_BUILD_DIR)/lib2dir:
	mkdir -p $(LOCAL_BUILD_DIR)/lib2dir

$(LOCAL_BUILD_DIR)/lib1dir/lib.o: $(LOCAL_SRC_DIR)/lib1.s $(LOCAL_BUILD_DIR)/lib1dir
	${AS} -o $(LOCAL_BUILD_DIR)/lib1dir/lib.o $(LOCAL_SRC_DIR)/lib1.s

$(LOCAL_BUILD_DIR)/lib2dir/lib.o: $(LOCAL_SRC_DIR)/lib2.s $(LOCAL_BUILD_DIR)/lib2dir
	${AS} -o $(LOCAL_BUILD_DIR)/lib2dir/lib.o $(LOCAL_SRC_DIR)/lib2.s

$(LOCAL_BUILD_DIR)/lib1.a: $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib1dir/lib.o
	ar rcs $(LOCAL_BUILD_DIR)/lib1.a $(LOCAL_BUILD_DIR)/lib1dir/lib.o

$(LOCAL_BUILD_DIR)/lib2.a: $(LOCAL_BUILD_DIR)/lib2.o $(LOCAL_BUILD_DIR)/lib2dir/lib.o
	ar rcs $(LOCAL_BUILD_DIR)/lib2.a $(LOCAL_BUILD_DIR)/lib2dir/lib.o

$(LOCAL_BUILD_DIR)/main:  $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.a $(LOCAL_BUILD_DIR)/lib2.a ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L$(LOCAL_BUILD_DIR) -l1 -l2 -o $(LOCAL_BUILD_DIR)/main -static

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main
	echo "lib-two-libs-with-same-object-filename-inside"

clean:
	@rm -Rf $(LOCAL_BUILD_DIR)/lib1dir
	@rm -Rf $(LOCAL_BUILD_DIR)/lib2dir
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/main

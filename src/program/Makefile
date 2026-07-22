BUILD_DIR := build
NATIVE_DIR := build-native

.PHONY: all configure native run-emu run-native clean

all: configure
	$(MAKE) -C $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/riscv-toolchain.cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5

native:
	cmake -S . -B $(NATIVE_DIR) -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	$(MAKE) -C $(NATIVE_DIR)

run-emu: all
	qemu-riscv64 $(BUILD_DIR)/program

run-native: native
	./$(NATIVE_DIR)/program

clean:
	rm -rf $(BUILD_DIR) $(NATIVE_DIR)

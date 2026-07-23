BUILD_DIR := build
NATIVE_DIR := build-native

QEMU_SYSTEM ?= qemu-system-riscv64
QEMU_FLAGS := -machine virt -nographic -bios none -kernel

.PHONY: all configure native run-emu run-native test clean

all: configure
	$(MAKE) -C $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/riscv-toolchain.cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5

native:
	cmake -S . -B $(NATIVE_DIR) -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	$(MAKE) -C $(NATIVE_DIR)

run-emu: all
	$(QEMU_SYSTEM) $(QEMU_FLAGS) $(BUILD_DIR)/program

run-native: native
	./$(NATIVE_DIR)/program

test: native
	ctest --test-dir $(NATIVE_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR) $(NATIVE_DIR)

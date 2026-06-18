BUILD_DIR := build

.PHONY: all configure clean

all: configure
	$(MAKE) -C $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/riscv-toolchain.cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5

clean:
	rm -rf $(BUILD_DIR)

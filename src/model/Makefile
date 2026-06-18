BUILD_DIR := build

.PHONY: all configure clean run

all: configure
	$(MAKE) -C $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_POLICY_VERSION_MINIMUM=3.5

clean:
	rm -rf $(BUILD_DIR)

run: all
	./$(BUILD_DIR)/sim

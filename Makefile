# Thin wrapper around CMake/CTest.
#
#   make          compile cbounded (normal build in build/)
#   make test     build and run the unit tests with ASan + UBSan (in build-test/)
#   make clean    remove both build directories

BUILD_DIR      ?= build
TEST_BUILD_DIR ?= build-test
CMAKE          ?= cmake
CTEST          ?= ctest
JOBS           ?= $(shell nproc)

.PHONY: all test clean

all:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCBOUNDED_SANITIZE=OFF
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)

# Separate build directory so switching between `make` and `make test`
# does not force a full rebuild with/without sanitizers each time.
test:
	$(CMAKE) -S . -B $(TEST_BUILD_DIR) -DCBOUNDED_SANITIZE=ON
	$(CMAKE) --build $(TEST_BUILD_DIR) -j $(JOBS)
	$(CTEST) --test-dir $(TEST_BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR) $(TEST_BUILD_DIR)

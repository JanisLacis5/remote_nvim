.DEFAULT_GOAL := release

ifeq ($(OS),Windows_NT)
DEFAULT_PLATFORM := windows
else
HOST_SYSTEM := $(shell uname -s)
ifeq ($(HOST_SYSTEM),Darwin)
DEFAULT_PLATFORM := mac
else ifeq ($(HOST_SYSTEM),Linux)
DEFAULT_PLATFORM := linux
else
DEFAULT_PLATFORM := linux
endif
endif

PLATFORM ?= $(DEFAULT_PLATFORM)
BUILD_ROOT := build/$(PLATFORM)

.PHONY: release debug debugaddr debugthread test testaddr linux mac windows clean

release:
	cmake -B $(BUILD_ROOT)/release \
		-DCMAKE_BUILD_TYPE=Release \
		-DPLATFORM=$(PLATFORM) \
		-DSANITIZER=none
	cmake --build $(BUILD_ROOT)/release --parallel

debug:
	cmake -B $(BUILD_ROOT)/debug \
		-DCMAKE_BUILD_TYPE=Debug \
		-DPLATFORM=$(PLATFORM) \
		-DSANITIZER=none
	cmake --build $(BUILD_ROOT)/debug --parallel

debugaddr:
	cmake -B $(BUILD_ROOT)/debugaddr \
		-DCMAKE_BUILD_TYPE=Debug \
		-DPLATFORM=$(PLATFORM) \
		-DSANITIZER=address
	cmake --build $(BUILD_ROOT)/debugaddr --parallel

debugthread:
	cmake -B $(BUILD_ROOT)/debugthread \
		-DCMAKE_BUILD_TYPE=Debug \
		-DPLATFORM=$(PLATFORM) \
		-DSANITIZER=thread
	cmake --build $(BUILD_ROOT)/debugthread --parallel

linux mac windows:
	$(MAKE) release PLATFORM=$@

test: debug
	ctest --test-dir $(BUILD_ROOT)/debug --output-on-failure $(CTEST_ARGS)

testaddr: debugaddr
	ctest --test-dir $(BUILD_ROOT)/debugaddr --output-on-failure $(CTEST_ARGS)

format:
	cmake --build build/linux/release --target format

clean:
	rm -rf build

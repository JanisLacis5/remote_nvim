.DEFAULT_GOAL := release

ifeq ($(OS),Windows_NT)
DEFAULT_PLATFORM := windows
else
HOST_SYSTEM := $(shell uname -s)
ifeq ($(HOST_SYSTEM),Darwin)
DEFAULT_PLATFORM := mac
else
DEFAULT_PLATFORM := linux
endif
endif

PLATFORM ?= $(DEFAULT_PLATFORM)
BUILD_ROOT := build/$(PLATFORM)

.PHONY: release debug debugaddr debugthread test testaddr linux mac windows format clean

release: BUILD_TYPE = Release
debug debugaddr debugthread: BUILD_TYPE = Debug
release debug: BUILD_SANITIZER = none
debugaddr: BUILD_SANITIZER = address
debugthread: BUILD_SANITIZER = thread

release debug debugaddr debugthread:
	cmake -B $(BUILD_ROOT)/$@ \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DPLATFORM=$(PLATFORM) \
		-DSANITIZER=$(BUILD_SANITIZER)
	cmake --build $(BUILD_ROOT)/$@ --parallel

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

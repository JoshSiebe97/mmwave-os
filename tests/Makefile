# tests/Makefile
#
# Host-side test build using Unity framework.
# Compiles against NuttX stubs — no real NuttX or ESP32 toolchain needed.
#
# Usage:
#   make              Build and run all tests
#   make test         Same as above
#   make test_parser  Build and run parser tests only
#   make clean        Remove build artifacts

# ---- Toolchain ----

CC      ?= cc
CFLAGS   = -Wall -Wextra -Werror -std=c11 -g -O0
CFLAGS  += -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
# macOS _IOW produces unsigned long values that overflow int switch cases;
# this is a host-vs-NuttX platform difference, not a real bug.
CFLAGS  += -Wno-switch

# ---- Paths (relative to this Makefile, which lives in tests/) ----

ROOT     = ..
STUBS    = stubs
UNITY    = unity
HELPERS  = helpers

# Include stubs FIRST so they shadow real NuttX headers
INCLUDES = -I$(STUBS) -I$(ROOT) -I.

# ---- Unity source ----

UNITY_SRC = $(UNITY)/unity.c

# ---- Build directory ----

BUILD    = build

# ---- Test binaries ----

TESTS    = $(BUILD)/test_parser \
           $(BUILD)/test_data_extract \
           $(BUILD)/test_ha_format

# ---- Default target ----

.PHONY: all test clean

all: test

test: $(TESTS)
	@echo ""
	@echo "═══════════════════════════════════"
	@echo "  Running mmWave OS test suite"
	@echo "═══════════════════════════════════"
	@echo ""
	@failures=0; \
	for t in $(TESTS); do \
		echo "--- $$t ---"; \
		./$$t || failures=$$((failures + 1)); \
		echo ""; \
	done; \
	echo "═══════════════════════════════════"; \
	if [ $$failures -eq 0 ]; then \
		echo "  All test suites passed."; \
	else \
		echo "  $$failures test suite(s) FAILED."; \
		exit 1; \
	fi; \
	echo "═══════════════════════════════════"

# ---- Build directory creation ----

$(BUILD):
	mkdir -p $(BUILD)

# ---- Individual test builds ----

$(BUILD)/test_parser: test_parser.c $(UNITY_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(BUILD)/test_data_extract: test_data_extract.c $(UNITY_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(BUILD)/test_ha_format: test_ha_format.c $(UNITY_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# ---- Convenience targets ----

.PHONY: test_parser test_data_extract test_ha_format

test_parser: $(BUILD)/test_parser
	./$(BUILD)/test_parser

test_data_extract: $(BUILD)/test_data_extract
	./$(BUILD)/test_data_extract

test_ha_format: $(BUILD)/test_ha_format
	./$(BUILD)/test_ha_format

# ---- Clean ----

clean:
	rm -rf $(BUILD)

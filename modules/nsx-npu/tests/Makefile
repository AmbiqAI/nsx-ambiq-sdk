# Host-native unit tests for nsx_npu_timebase.c (see nsx_npu_timebase_tests.c
# for scope/rationale). Runs with the system's native gcc -- no ARM
# cross-compiler, no ambiqsuite checkout, no FPGA/silicon required.
#
# Usage:
#   make            # build ./build/nsx_npu_timebase_tests
#   make test       # build and run it
#   make clean

CC       ?= gcc
BUILDDIR := build
TARGET   := $(BUILDDIR)/nsx_npu_timebase_tests

SRC_DIR      := ..
INCLUDES_API := $(SRC_DIR)/includes-api
TB_SRC_DIR   := $(SRC_DIR)/src
TB_ATOMIQ_DIR:= $(SRC_DIR)/src/atomiq110

CFLAGS := -std=c11 -Wall -Wextra -g -O0 \
          -I mocks -I unity -I $(INCLUDES_API) -I $(TB_SRC_DIR)

SOURCES := unity/unity.c \
           mocks/mock_hal.c \
           $(TB_ATOMIQ_DIR)/nsx_npu_timebase.c \
           nsx_npu_timebase_tests.c \
           runner_main.c

# Flatten to build/<basename>.o -- one of SOURCES lives outside this
# directory (../src/atomiq110/...), so mirroring paths with patsubst would
# otherwise place its object under build/../src/..., i.e. outside $(BUILDDIR)
# entirely. No two sources share a basename, so flattening is unambiguous.
OBJECTS := $(addprefix $(BUILDDIR)/,$(notdir $(SOURCES:.c=.o)))

.PHONY: all test clean

all: $(TARGET)

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

vpath %.c unity mocks $(TB_ATOMIQ_DIR) .

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)

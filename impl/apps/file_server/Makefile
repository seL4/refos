#
# Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Targets
TARGETS := file_server.bin

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*/*.c))

OFILES := archive.o
NK_CFLAGS += -O2

# Libraries required to build the target
LIBS := c sel4 elf refossys refos cpio datastruct

# Custom linker script
NK_LDFLAGS += -T $(SOURCE_DIR)/linker.lds

include $(SEL4_COMMON)/common.mk

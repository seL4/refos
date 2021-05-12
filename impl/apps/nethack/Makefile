#
# Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Targets
TARGETS := nethack.bin

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*/*/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*/*/*/*.c))
HDRFILES += $(wildcard ${SOURCE_DIR}/src/nethack-3.4.3/include/*)

NK_CFLAGS += -D_BSD_SOURCE -O2

# Nethack configuration defines.
NK_CFLAGS += -DTTY_GRAPHICS -DANSI_DEFAULT -DDLB -DNO_FILE_LINKS- -DDLBLIB

# We also disable warnings when compiling nethack; there are a lot.
NK_CFLAGS += -w

# Libraries required to build the target
LIBS := c sel4 refossys refos datastruct

# Custom linker script
NK_LDFLAGS += -T $(SOURCE_DIR)/linker.lds

include $(SEL4_COMMON)/common.mk

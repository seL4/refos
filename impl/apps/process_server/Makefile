#
# Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Targets
TARGETS := $(notdir $(SOURCE_DIR)).bin

# Entry Point
ENTRY_POINT := _sel4_start

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*/*/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/plat/${PLAT}/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/arch/${ARCH}/*.c))

OFILES := archive.o

# We want to run C99
NK_CFLAGS += -std=gnu99 -O2

# Libraries required to build the target
LIBS := c sel4 sel4muslcsys sel4allocman sel4platsupport sel4vka elf cpio refos sel4utils \
      datastruct sel4debug sel4simple utils platsupport sel4vspace

LIBS += sel4simple-default

HDRFILES += $(wildcard ${SOURCE_DIR}/*) \


include $(SEL4_COMMON)/common.mk

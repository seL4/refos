#
# Copyright 2016, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(D61_BSD)
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
      datastruct sel4simple utils platsupport sel4vspace

# Set up correct simple lib for our kernel
ifdef CONFIG_KERNEL_STABLE
LIBS += sel4simple-stable
else
LIBS += sel4simple-default
endif

HDRFILES += $(wildcard ${SOURCE_DIR}/*) \


include $(SEL4_COMMON)/common.mk

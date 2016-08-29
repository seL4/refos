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
TARGETS := test_os.bin

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
ASMFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/crt/arch-${ARCH}/crt0.S))

NK_CFLAGS += -O2

# Libraries required to build the target
LIBS := muslc sel4 refossys refos datastruct

# Custom linker script
NK_LDFLAGS += -T $(SOURCE_DIR)/linker.lds

include $(SEL4_COMMON)/common.mk

#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

VERSION = 2
PATCHLEVEL = 0
SUBLEVEL = 0
EXTRAVERSION = 1

lib-dirs:=libs

# The main target we want to generate
all: generate-rpc refos

-include .config
export CONFIG_ELF_LOADER_USE_SEL4C
export CONFIG_ELF_LOADER_USE_MUSLC
include tools/common/project.mk

# Build CPIO Archives.
$(BUILD_BASE)/process_server/archive.o: export TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
$(BUILD_BASE)/process_server/archive.o: file_server test_os selfloader console_server
	$(Q)mkdir -p $(dir $@)
	@echo "[CPIO] $@"
	$(Q)${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive \
		$(patsubst %, ${STAGE_BASE}/bin/%,$^)
	@echo "[CPIO] done."

$(BUILD_BASE)/file_server/archive.o: export TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
$(BUILD_BASE)/file_server/archive.o: $(filter-out selfloader process_server file_server test_os \
							           console_server,$(apps))
	$(Q)mkdir -p $(dir $@)
	@echo "[CPIO] $@"
	$(Q)${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive \
		$(patsubst %, ${STAGE_BASE}/bin/%,$^) \
		$(wildcard apps/file_server/files/*)
	@echo "[CPIO] done."

# RefOS ARM build command.
ifeq (${CONFIG_ARCH_ARM},y)
refos: export TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
refos: $(BUILD_BASE)/file_server/archive.o $(BUILD_BASE)/process_server/archive.o process_server \
        common elfloader kernel_elf
	@echo "Generating boot image: ${IMAGE_ROOT}/refos-image"
	@${SEL4_COMMON}/elfloader/gen_boot_image.sh \
		${STAGE_BASE}/kernel.elf \
		${STAGE_BASE}/bin/process_server \
		${IMAGE_ROOT}/refos-image
endif

# RefOS IA32 build command.
ifeq (${CONFIG_ARCH_IA32},y)
refos: export PATH := $(STAGE_ROOT)/dite:$(PATH)
refos: $(BUILD_BASE)/file_server/archive.o $(BUILD_BASE)/process_server/archive.o process_server\
		common app-images
	@echo "[STAGE] ${IMAGE_ROOT}/refos-image"
	cp -f ${STAGE_BASE}/bin/process_server ${IMAGE_ROOT}/refos-image
endif

# Generate RPC stubs.
generate-rpc:
	./refos_cidl_compile make proc
	./refos_cidl_compile make name
	./refos_cidl_compile make serv
	./refos_cidl_compile make data

# Clean RPC stubs.
clean-rpc:
	./refos_cidl_compile clean proc
	./refos_cidl_compile clean name
	./refos_cidl_compile clean serv
	./refos_cidl_compile clean data

# Misc helper targets.
cscope: clean
	@echo "[CSCOPE] cscope.out"
	@cscope -b -R -k

docs: generate-rpc clean
	@echo "[DOXYGEN] Generating docs..."
	cd docs/; make
	@echo "[DOXYGEN] OK. Please point your browser at file://$(PWD)/docs/html/index.html"

design:
	@echo "[DESIGN] Generating RefOS procotol design document..."
	cd projects/refos/design/; make clean; make
	@echo "[DESIGN] OK. Please take a look at file://$(PWD)/projects/refos/design/paper.pdf"

# QEMU simulation helpers.
simulate-kzm:
	qemu-system-arm -nographic -M kzm \
		-kernel images/refos-image

simulate-ia32:
	qemu-system-i386 \
		-m 512 -nographic -kernel images/kernel-ia32-pc99 \
		-initrd images/refos-image

simulate-ia32-graphics:
	qemu-system-i386 \
		-m 512 -kernel images/kernel-ia32-pc99 \
		-initrd images/refos-image

# Help
.PHONY: help
help:
	@echo "RefOS - Reference multi-server OS on seL4."
	@echo ""
	@echo " make help            - Show this help test."
	@echo " make menuconfig      - Select build configuration via menus."
	@echo " make <defconfig>     - Apply one of the default configurations. See"
	@echo "                        below for valid configurations."
	@echo " make silentoldconfig - Update configuration with the defaults of any"
	@echo "                        newly introduced settings."
	@echo " make                 - Build everything with the current configuration."
	@echo " make generate-rpc    - Generate RPC stubs from XML specs."
	@echo " make clean-rpc       - Delete generated RPC stubs."
	@echo " make refos           - Build RefOS without re-generating RPC stub code."
	@echo " make cscope          - Build cscope.out index file using cscope."
	@echo " make docs            - Build docs/html/ Doxygen code documentation."
	@echo " make design          - Build protocol design document."
	@echo ""
	@echo "Valid default configurations are:"
	@ls -1 configs | sed -e 's/\(.*\)/\t\1/g'
	@echo ""


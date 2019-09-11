TOP_DIR ?= $(shell pwd)/../
HDL_DIR ?= $(TOP_DIR)/hdl

DRIVER_NAME := spec-fmc-carrier
VERSION := $(shell git describe --abbrev=0)
DIR_NAME := $(DRIVER_NAME)-$(VERSION)
KEEP_TEMP ?= n
BUILD ?= $(abspath build)
BUILD_DKMS := $(BUILD)/dkms
BUILD_DKMSSOURCE := $(BUILD_DKMS)/source
BUILD_DKMSTREE := $(BUILD_DKMS)/tree


DKMS_OPT := --dkmstree $(BUILD_DKMSTREE) -m $(DRIVER_NAME)/$(VERSION)

all: kernel

CHEBY ?= /usr/bin/cheby

spec-core-fpga.h:
	$(CHEBY) --gen-c -i $(HDL_DIR)/rtl/spec_base_regs.cheby > /tmp/$@

kernel: dkms-tar dkms-rpm

dkms-tree:
	@mkdir -p $(BUILD_DKMSSOURCE)
	@mkdir -p $(BUILD_DKMSTREE)

dkms-src: dkms-tree spec-core-fpga.h
	$(eval $@_src := $(shell git ls-tree -r --name-only HEAD $(TOP_DIR) | grep "kernel" | tr '\n' ' '))
	$(eval $@_dir := $(BUILD_DKMSSOURCE)/$(DRIVER_NAME)-$(VERSION))

	@mkdir -p $($@_dir)/platform_data
	@mv /tmp/spec-core-fpga.h $($@_dir)
	@cp -a $($@_src) $(TOP_DIR)/distribution/dkms.conf $($@_dir)
	@mv $($@_dir)/gn412x-gpio.h $($@_dir)/platform_data
	@cp -a $(TOP_DIR)/LICENSES/GPL-2.0.txt $($@_dir)/LICENSE
	@sed -r -i -e "s/^VERSION\s=\s.*/VERSION = $(VERSION)/" $($@_dir)/Makefile
	@sed -r -i -e "s/@PKGNAME@/$(DRIVER_NAME)/" $($@_dir)/dkms.conf
	@sed -r -i -e "s/@PKGVER@/$(VERSION)/" $($@_dir)/dkms.conf


dkms-add: dkms-src
	@dkms add $(DKMS_OPT) --sourcetree $(BUILD_DKMSSOURCE)

dkms-tar: dkms-add
	@dkms mktarball $(DKMS_OPT) --source-only

dkms-rpm: dkms-add
	@dkms mkrpm $(DKMS_OPT) --source-only

clean:
	@rm -rf $(BUILD)

.PHONY: dkmstree dkms-add kernel-dkms-tar

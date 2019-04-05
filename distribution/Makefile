TOP_DIR ?= ../
DRIVER_NAME := spec
VERSION := $(shell git describe --abbrev=0)
DIR_NAME := $(DRIVER_NAME)-$(VERSION)
KEEP_TEMP ?= n
BUILD ?= $(abspath build)
BUILD_DKMS := $(BUILD)/dkms
BUILD_DKMSSOURCE := $(BUILD_DKMS)/source
BUILD_DKMSTREE := $(BUILD_DKMS)/tree


SRC := $(TOP_DIR)/Makefile
SRC += $(TOP_DIR)/Makefile

DKMS_OPT := --dkmstree $(BUILD_DKMSTREE) -m $(DRIVER_NAME)/$(VERSION)

all: kernel

kernel: dkms-tar dkms-rpm

dkms-tree:
	@mkdir -p $(BUILD_DKMSSOURCE)
	@mkdir -p $(BUILD_DKMSTREE)

dkms-src: dkms-tree
	$(eval $@_src := $(shell git ls-tree -r --name-only HEAD $(TOP_DIR) | grep "kernel" | tr '\n' ' '))
	$(eval $@_dir := $(BUILD_DKMSSOURCE)/$(DRIVER_NAME)-$(VERSION))

	@mkdir -p $($@_dir)
	@cp $($@_src) $(TOP_DIR)/distribution/dkms.conf $($@_dir)
	@cp $(TOP_DIR)/LICENSES/GPL-2.0.txt $($@_dir)/LICENSE
	@sed -r -i -e "s/^VERSION\s=\s.*/VERSION = $(VERSION)/" $($@_dir)/Makefile
	@sed -r -i -e "s/PACKAGE_NAME=.*/PACKAGE_NAME=$(DRIVER_NAME)/" $($@_dir)/dkms.conf
	@sed -r -i -e "s/PACKAGE_VERSION=.*/PACKAGE_VERSION=$(VERSION)/" $($@_dir)/dkms.conf

dkms-add: dkms-src
	@dkms add $(DKMS_OPT) --sourcetree $(BUILD_DKMSSOURCE)

dkms-tar: dkms-add
	@dkms mktarball $(DKMS_OPT) --source-only

dkms-rpm: dkms-add
	@dkms mkrpm $(DKMS_OPT) --source-only

clean:
	@rm -rf $(BUILD)

.PHONY: dkmstree dkms-add kernel-dkms-tar

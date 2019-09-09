-include Makefile.specific
# include parent_common.mk for buildsystem's defines
#use absolute path for REPO_PARENT
REPO_PARENT ?= $(shell /bin/pwd)/../..
-include $(REPO_PARENT)/parent_common.mk

KVERSION ?= $(shell uname -r)
LINUX ?= /lib/modules/$(KVERSION)/build

TOP_DIR ?= $(shell pwd)/../..
HDL_DIR ?= $(TOP_DIR)/hdl

CONFIG_FPGA_MGR_BACKPORT_PATH_ABS ?= $(abspath $(CONFIG_FPGA_MGR_BACKPORT_PATH))
FMC_ABS ?= $(abspath $(FMC))
I2C_ABS ?= $(abspath $(I2C))

VERSION = $(shell git describe --dirty --long --tags)

CHEBY ?= /usr/bin/cheby

all: modules

.PHONY: all modules clean help install modules_install spec-core-fpga.h

spec-core-fpga.h:
ifeq ($(DKMS), 0)
	$(CHEBY) --gen-c -i $(HDL_DIR)/rtl/spec_base_regs.cheby > $@
endif

modules help install modules_install: spec-core-fpga.h
	$(MAKE) -C $(LINUX) M=$(shell pwd) VERSION=$(VERSION) CONFIG_FPGA_MGR_BACKPORT_PATH_ABS=$(CONFIG_FPGA_MGR_BACKPORT_PATH_ABS) CONFIG_FPGA_MGR_BACKPORT=$(CONFIG_FPGA_MGR_BACKPORT) FMC_ABS=$(FMC_ABS) I2C_ABS=$(I2C_ABS) $@

# be able to run the "clean" rule even if $(LINUX) is not valid
clean:
	rm -rf *.o *~  .*.cmd *.ko *.mod.c .tmp_versions Module.symvers \
                         Module.markers modules.order spec-core-fpga.h

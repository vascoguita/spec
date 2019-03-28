# include parent_common.mk for buildsystem's defines
#use absolute path for REPO_PARENT
REPO_PARENT ?= $(shell /bin/pwd)/../..
-include $(REPO_PARENT)/parent_common.mk

LINUX ?= /lib/modules/$(shell uname -r)/build

FPGA_MGR_ABS ?= $(abspath $(FPGA_MGR))

FMC_ABS ?= $(abspath $(FMC))

I2C_ABS ?= $(abspath $(I2C))

GIT_VERSION = $(shell git describe --dirty --long --tags)
export GIT_VERSION

all: modules

.PHONY: all modules clean help install modules_install

modules help install modules_install:
	$(MAKE) -C $(LINUX) M=$(shell pwd) GIT_VERSION=$(GIT_VERSION) FPGA_MGR_ABS=$(FPGA_MGR_ABS) $@

# be able to run the "clean" rule even if $(LINUX) is not valid
clean:
	rm -rf *.o *~  .*.cmd *.ko *.mod.c .tmp_versions Module.symvers \
                         Module.markers modules.order

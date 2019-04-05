# include parent_common.mk for buildsystem's defines
#use absolute path for REPO_PARENT
REPO_PARENT ?= $(shell /bin/pwd)/../..
-include $(REPO_PARENT)/parent_common.mk

LINUX ?= /lib/modules/$(shell uname -r)/build

CONFIG_FPGA_MGR_BACKPORT_PATH_ABS ?= $(abspath $(CONFIG_FPGA_MGR_BACKPORT_PATH))

GIT_VERSION = $(shell git describe --dirty --long --tags)

all: modules

.PHONY: all modules clean help install modules_install

modules help install modules_install:
	$(MAKE) -C $(LINUX) M=$(shell pwd) GIT_VERSION=$(GIT_VERSION) CONFIG_FPGA_MGR_BACKPORT_PATH_ABS=$(CONFIG_FPGA_MGR_BACKPORT_PATH_ABS) CONFIG_FPGA_MGR_BACKPORT=$(CONFIG_FPGA_MGR_BACKPORT) $@

# be able to run the "clean" rule even if $(LINUX) is not valid
clean:
	rm -rf *.o *~  .*.cmd *.ko *.mod.c .tmp_versions Module.symvers \
                         Module.markers modules.order

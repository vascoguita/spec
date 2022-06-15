# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CC0-1.0

-include Makefile.specific
-include $(REPO_PARENT)/parent_common.mk

TOP_DIR ?= $(shell pwd)/../..
include $(TOP_DIR)/common.mk

KVERSION ?= $(shell uname -r)
LINUX ?= /lib/modules/$(KVERSION)/build
INSTALL_MOD_PATH ?=
INSTALL_MOD_DIR ?= extra/cern/spec-fmc-carrier


ifndef CONFIG_FPGA_MGR_BACKPORT
ifeq ($(shell test $(shell uname -p) \> "4.4"), 0)
CONFIG_FPGA_MGR_BACKPORT := n
else
CONFIG_FPGA_MGR_BACKPORT := y
endif
endif

ifdef REPO_PARENT
FPGA_MGR ?= $(REPO_PARENT)/fpga-mgr-backport/
FMC ?= $(REPO_PARENT)/fmc-sw/
I2C ?= $(REPO_PARENT)/general-cores/software/i2c-ocores/
SPI ?= $(REPO_PARENT)/general-cores/software/spi-ocores/
endif

ifndef FPGA_MGR
$(error "Missing FPGA_MGR environment variable")
endif
ifndef FMC
$(error "Missing FMC environment variable")
endif
ifndef I2C
$(error "Missing I2C environment variable")
endif
ifndef SPI
$(error "Missing SPI environment variable")
endif

FPGA_MGR_ABS ?= $(abspath $(FPGA_MGR))
FMC_ABS ?= $(abspath $(FMC))
I2C_ABS ?= $(abspath $(I2C))
SPI_ABS ?= $(abspath $(SPI))

CPPCHECK ?= cppcheck

all: modules

.PHONY: all modules clean help install modules_install coccicheck spec-core-fpga.h cppcheck

help coccicheck:
	@$(MAKE) -C $(LINUX) M=$(shell pwd) $@

headers:
	$(MAKE) -C $(HDR_DIR) $@

modules: headers
	$(MAKE) -C $(LINUX) M=$(shell pwd) \
		VERSION=$(GIT_VERSION) \
		FPGA_MGR_ABS=$(FPGA_MGR_ABS) \
		CONFIG_FPGA_MGR_BACKPORT=$(CONFIG_FPGA_MGR_BACKPORT) \
		FMC_ABS=$(FMC_ABS) \
		I2C_ABS=$(I2C_ABS) \
		SPI_ABS=$(SPI_ABS) \
		$@

install: modules_install
modules_install: modules
	@$(MAKE) -C $(LINUX) M=$(shell pwd) \
		INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) \
		INSTALL_MOD_DIR=$(INSTALL_MOD_DIR) \
		$@

clean:
	@rm -f spec-core-fpga.h
	@$(MAKE) -C $(LINUX) M=$(shell pwd) $@


INCLUDE := -I./
cppcheck:
	$(CPPCHECK) -q $(INCLUDE) --suppress=missingIncludeSystem --enable=all *.c *.h  --error-exitcode=1

-include Makefile.specific
-include $(REPO_PARENT)/parent_common.mk

KVERSION ?= $(shell uname -r)
LINUX ?= /lib/modules/$(KVERSION)/build

TOP_DIR ?= $(shell pwd)/../..
HDL_DIR ?= $(TOP_DIR)/hdl
DKMS ?= 0


ifndef (CONFIG_FPGA_MGR_BACKPORT)
ifeq ($(shell test $(shell uname -p) \> "4.4"), 0)
CONFIG_FPGA_MGR_BACKPORT := n
else
CONFIG_FPGA_MGR_BACKPORT := y
endif
endif

ifeq ($(DKMS), 1)
FPGA_MGR_VERSION ?= $(shell basename $(shell ls -d $(DKMSTREE)/fpga_mgr/* | grep -E "\/[0-9]+\.[0-9]+\.[0-9]+" | sort -V | tail -n 1))
FMC_VERSION ?= $(shell basename $(shell ls -d $(DKMSTREE)/fmc/* | grep -E "\/[0-9]+\.[0-9]+\.[0-9]+" | sort -V | tail -n 1))
I2C_VERSION ?= $(shell basename $(shell ls -d $(DKMSTREE)/i2c-ocores/* | grep -E "\/[0-9]+\.[0-9]+\.[0-9]+" | sort -V | tail -n 1))
SPI_VERSION ?= $(shell basename $(shell ls -d $(DKMSTREE)/spi-ocores/* | grep -E "\/[0-9]+\.[0-9]+\.[0-9]+" | sort -V | tail -n 1))

FPGA_MGR ?= $(DKMSTREE)/fpga_mgr/$(FPGA_MGR_VERSION)/source
FMC ?= $(DKMSTREE)/fmc/$(FMC_VERSION)/source
I2C ?= $(DKMSTREE)/i2c-ocores/$(I2C_VERSION)/source
SPI ?= $(DKMSTREE)/spi-ocores/$(SPI_VERSION)/source
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

VERSION = $(shell git describe --dirty --long --tags)

CHEBY ?= /usr/bin/cheby

all: modules

.PHONY: all modules clean help install modules_install coccicheck spec-core-fpga.h

spec-core-fpga.h:
ifeq ($(DKMS), 0)
	$(CHEBY) --gen-c -i $(HDL_DIR)/rtl/spec_base_regs.cheby > $@
endif

clean-spec-core-fpga.h:
ifeq ($(DKMS), 0)
	rm -f spec-core-fpga.h
endif

install: modules_install
clean: clean-spec-core-fpga.h
modules help coccicheck modules_install clean: spec-core-fpga.h
	$(MAKE) -C $(LINUX) M=$(shell pwd) \
		VERSION=$(VERSION) \
		FPGA_MGR_ABS=$(FPGA_MGR_ABS) \
		CONFIG_FPGA_MGR_BACKPORT=$(CONFIG_FPGA_MGR_BACKPORT) \
		FMC_ABS=$(FMC_ABS) \
		I2C_ABS=$(I2C_ABS) \
		SPI_ABS=$(SPI_ABS) \
		$@

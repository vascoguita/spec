# SPDX-FileCopyrightText: 2022 CERN (home.cern)
#
# SPDX-License-Identifier: LGPL-2.1-or-later

TOP_DIR ?= $(shell pwd)/../..
include $(TOP_DIR)/common.mk

#
# Tools
#
CHEBY ?= cheby

#
# Install variable
#
DESTDIR ?= /usr/local
HDRDIR ?= $(DESTDIR)/include


all: headers

headers: uapi/linux/spec-core-fpga.h

uapi/linux/spec-core-fpga.h:
	$(CHEBY) --gen-c -i $(HDL_DIR)/rtl/spec_base_regs.cheby > $@

install: headers
	install -d $(HDRDIR)/linux
	install -D uapi/linux/spec.h $(HDRDIR)/linux
	install -D uapi/linux/spec-core-fpga.h $(HDRDIR)/linux

clean:
	rm -f uapi/linux/spec-core-fpga.h

.PHONY: install all clean

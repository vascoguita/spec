# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
TOPDIR = $(shell pwd)/..
REPO_PARENT ?= $(TOPDIR)/..

-include Makefile.specific
-include $(REPO_PARENT)/parent_common.mk

DIRS = include kernel tools

.PHONY: all clean modules install modules_install coccicheck cppcheck $(DIRS)

all install modules_install coccicheck modules: $(DIRS)

modules: TARGET = modules
coccicheck: TARGET = coccicheck
install: TARGET = install
modules_install: TARGET = modules_install

cppcheck:
	$(MAKE) -C tools $@

headers:
	$(MAKE) -C include $@

$(DIRS): headers
	$(MAKE) -C $@ $(ENV_VAR) $(TARGET)

clean:
	$(MAKE) -C include $@
	$(MAKE) -C kernel $@
	$(MAKE) -C tools $@

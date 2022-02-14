# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (C) 2020 CERN

# If it exists includes Makefile.specific. In this Makefile, you should put
# specific Makefile code that you want to run before this. For example,
# build a particular environment.
-include Makefile.specific
TOP_DIR ?= $(shell pwd)/../..
include $(TOP_DIR)/common.mk

#
# Install variable
#
DESTDIR ?= /usr/local/
BINDIR ?= $(DESTDIR)/bin

TOOLS = spec-firmware-version

#
# Compiler options
#
INCLUDE := -I. -I../kernel -I../include/uapi
CFLAGS = -ggdb $(INCLUDE) -Wall -Werror $(EXTRACFLAGS)
CFLAGS += -DGIT_VERSION="\"$(GIT_VERSION)\""

CPPCHECK ?= cppcheck

all: $(TOOLS)

headers:
	$(MAKE) -C $(HDR_DIR) $@

%: %.c headers
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@


clean:
	rm -f $(TOOLS)

install:
	install -d $(BINDIR)
	install -D $(TOOLS) $(BINDIR)

cppcheck:
	$(CPPCHECK) -q $(INCLUDE) --suppress=missingIncludeSystem --enable=all *.c *.h  --error-exitcode=1

.PHONY: cppcheck

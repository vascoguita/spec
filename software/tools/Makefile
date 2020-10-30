# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (C) 2020 CERN

# If it exists includes Makefile.specific. In this Makefile, you should put
# specific Makefile code that you want to run before this. For example,
# build a particular environment.
-include Makefile.specific

DESTDIR ?= /usr/local/

TOOLS = spec-firmware-version

INCLUDE := -I. -I../kernel -I../include/uapi
CFLAGS = -ggdb $(INCLUDE) -Wall -Werror $(EXTRACFLAGS)
GIT_VERSION := $(shell git describe --dirty --long --tags)
CFLAGS += -DGIT_VERSION="\"$(GIT_VERSION)\""

CPPCHECK ?= cppcheck

all: $(TOOLS)


$(TOOLS):

clean:
	@$(MAKE) -C $(LIB) $@
	rm -f $(TOOLS)

install:
	install -d $(DESTDIR)/bin
	install -D $(TOOLS) $(DESTDIR)/bin

cppcheck:
	$(CPPCHECK) -q $(INCLUDE) --suppress=missingIncludeSystem --enable=all *.c *.h  --error-exitcode=1

.PHONY=cppcheck

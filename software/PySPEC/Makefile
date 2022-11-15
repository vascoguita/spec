# SPDX-License-Identifier: LGPL-2.1-or-later
#
# SPDX-FileCopyrightText: 2020 CERN

-include Makefile.specific
TOP_DIR ?= $(shell pwd)/../..
include $(TOP_DIR)/common.mk

all: build
clean:
	rm -rf build

build:
	python3 -m build

install:
	pip3 install -e .

.PHONY: all clean install

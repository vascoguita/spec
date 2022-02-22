# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileCopyrightText: 2020 CERN

-include Makefile.specific
TOP_DIR ?= $(shell pwd)/../..
include $(TOP_DIR)/common.mk

all: build
clean:
	rm -rf build

build install:
	python3 setup.py $@

.PHONY: all clean install

HDL_DIR ?= $(TOP_DIR)/hdl
SW_DIR ?= $(TOP_DIR)/software
HDR_DIR ?= $(SW_DIR)/include
DST_DIR ?= $(TOP_DIR)/distribution

IS_GIT_TREE = $(shell git rev-parse --is-inside-work-tree 2> /dev/null)
ifeq ($(IS_GIT_TREE), true)
VERSION = $(shell git describe --abbrev=0 | tr -d 'v')
GIT_VERSION = $(shell git describe --dirty --long --tags)
else
ifndef VERSION
$(error "Missing VERSION")
endif
ifndef GIT_VERSION
$(error "Missing GIT_VERSION")
endif
endif

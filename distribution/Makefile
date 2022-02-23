# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CC0-1.0

TOP_DIR ?= $(shell pwd)/../
include $(TOP_DIR)/common.mk

NAME := spec-fmc-carrier
VERSION := $(shell git describe --abbrev=0 | tr -d 'v')
GIT_VERSION := $(shell git describe --always --dirty --long --tags)
DIR_NAME := $(NAME)-$(VERSION)

# temporary RPMBUILD directory
RPMBUILD_NAME := $(NAME)-$(VERSION)-rpmbuild
RPMBUILD := $(DST_DIR)/$(RPMBUILD_NAME)

# SOURCES
SOURCES ?= $(RPMBUILD)/SOURCES
TARGZ := $(SOURCES)/$(DIR_NAME).tar.gz
CHANGELOG := $(SOURCES)/CHANGELOG
# SPEC files
SPECS_DIR ?= $(RPMBUILD)/SPECS
SPEC_NAME := $(NAME).spec
SPEC ?= $(SPECS_DIR)/$(SPEC_NAME)

CHEBY ?= cheby
GIT ?= git

ifeq (,$(shell which $(GIT)))
$(error "Missing git")
endif

ifeq (,$(shell which $(CHEBY)))
$(error "Missing cheby")
endif


all: rpm

#
# Build compressed sources tarball
#
targz: $(TARGZ)
$(TARGZ):
	@mkdir -p $(SOURCES)
	@GIT_DIR=$(TOP_DIR)/.git git archive --format=tar.gz -o $@ --prefix=$(DIR_NAME)/ HEAD

#
# CHANGE LOG needs to be converted into RPM format
#
CHANGELOG: $(CHANGELOG)
$(CHANGELOG): $(TOP_DIR)/CHANGELOG.rst
	@mkdir -p $(SOURCES)
	@cp $< $@
	$(eval $@_pattern := ^([0-9.]+)\s-\s([0-9]{4}-[0-9]{2}-[0-9]{2})$)
	$(eval $@_replace := echo -e "\n"\\* `date --date="\2" "+%a %b %d %Y"` "\1")
	sed -r -i -e "/^[.]{2}/d" -e "/^\s{2}.*$$/d" $@
	sed -r -i -e "/Change Log/d" -e "/^(=|-|\s)*$$/d" $@
	sed -r -i -e 's,$($@_pattern),$($@_replace),e' $@

#
# The .spec file is good when we can define variables. With tools like
# koji it is not possible, so we need to create a .spec file without using
# package-specific variables.
#
$(SPEC): $(SPEC_NAME)
	@mkdir -p $(SPECS_DIR)
	cp $< $@
	@sed -i -e "s/%{?_version}/$(VERSION)/" $@
	@sed -i -e "s/%{?_git_version}/$(GIT_VERSION)/" $@

sources: $(TARGZ) $(CHANGELOG)

srpm: $(SPEC) sources
	@rpmbuild -bs \
		--define "_topdir $(RPMBUILD)" \
		--define '_sourcedir $(SOURCES)' \
		$<


#
# Build binary packages
#
rpm: $(SPEC) sources
ifdef LINUX
	rpmbuild -bb --define "_topdir $(RPMBUILD)" \
		     --define "_sourcedir $(SOURCES)" \
		     --define "with_linux $(LINUX)" \
		$<
else
	rpmbuild -bb --define "_topdir $(RPMBUILD)" --define '_sourcedir $(SOURCES)'  $<
endif

clean:
	@rm -rf $(RPMBUILD_NAME)*

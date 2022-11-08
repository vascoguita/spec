# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CC-BY-SA-4.0+

# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    = -Drelease=$(shell git describe) -Dversion=$(shell git describe | cut -d "-" -f 1 | tr -d "v")
SPHINXBUILD   = sphinx-build
SOURCEDIR     = .
BUILDDIR      = _build

CHEBY ?= cheby
CHEBY_FILES := spec_base_regs.cheby
CHEBY_DOC_FILES := $(CHEBY_FILES:.cheby=.rst)

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: clean help cheby_doc

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: cheby_doc
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

vpath %.cheby ../hdl/rtl
cheby_doc: $(CHEBY_DOC_FILES)
$(CHEBY_DOC_FILES): %.rst: %.cheby
	$(CHEBY) -i $< --gen-doc --doc rest > $@

clean:
	@rm -f $(CHEBY_DOC_FILES)
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

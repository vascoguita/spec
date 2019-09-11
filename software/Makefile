-include Makefile.specific

# include parent_common.mk for buildsystem's defines
#use absolute path for REPO_PARENT
REPO_PARENT ?= $(shell /bin/pwd)/..
-include $(REPO_PARENT)/parent_common.mk

DIRS = kernel

.PHONY: all clean modules install modules_install $(DIRS)

all clean modules install modules_install: $(DIRS)

clean: TARGET = clean
modules: TARGET = modules
install: TARGET = install
modules_install: TARGET = modules_install

ENV_VAR := CONFIG_FPGA_MGR_BACKPORT_PATH=$(CONFIG_FPGA_MGR_BACKPORT_PATH)
ENV_VAR += CONFIG_FPGA_MGR_BACKPORT=$(CONFIG_FPGA_MGR_BACKPORT)

$(DIRS):
	$(MAKE) -C $@ $(ENV_VAR) $(TARGET)

# include parent_common.mk for buildsystem's defines
#use absolute path for REPO_PARENT
REPO_PARENT ?= $(shell /bin/pwd)/..
-include $(REPO_PARENT)/parent_common.mk

DIRS = kernel doc

.PHONY: all clean modules install modules_install $(DIRS)

all clean modules install modules_install: $(DIRS)

clean: TARGET = clean
modules: TARGET = modules
install: TARGET = install
modules_install: TARGET = modules_install

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

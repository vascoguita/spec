-include Makefile.specific

# include parent_common.mk for buildsystem's defines
#use absolute path for REPO_PARENT
REPO_PARENT ?= $(shell /bin/pwd)/..
-include $(REPO_PARENT)/parent_common.mk

DIRS = kernel

.PHONY: all clean modules install modules_install coccicheck $(DIRS)

all clean : $(DIRS)
install modules_install coccicheck modules: kernel

clean: TARGET = clean
modules: TARGET = modules
coccicheck: TARGET = coccicheck
install: TARGET = install
modules_install: TARGET = modules_install

$(DIRS):
	$(MAKE) -C $@ $(ENV_VAR) $(TARGET)

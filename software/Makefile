-include Makefile.specific
-include $(REPO_PARENT)/parent_common.mk

DIRS = kernel tools

.PHONY: all clean modules install modules_install coccicheck cppcheck $(DIRS)

all clean : $(DIRS)
install modules_install coccicheck modules: kernel

clean: TARGET = clean
modules: TARGET = modules
coccicheck: TARGET = coccicheck
install: TARGET = install
modules_install: TARGET = modules_install
cppcheck:
	$(MAKE) -C tools $@

$(DIRS):
	$(MAKE) -C $@ $(ENV_VAR) $(TARGET)

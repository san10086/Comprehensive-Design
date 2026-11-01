#
# Top-level build for the mature ARM/DSP project layout.
#

EXBASE = .
include $(EXBASE)/products.mak

.DEFAULT_GOAL := all

.PHONY: all clean install help hrir hrir-check

PYTHON ?= python3
HRIR_SOURCE ?= assets/hrir/mit_kemar

hrir:
	$(PYTHON) tools/generate_hrir.py --source "$(HRIR_SOURCE)"

hrir-check:
	$(PYTHON) tools/generate_hrir.py --source "$(HRIR_SOURCE)" --check

all:
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(MAKE) -C host all
	$(MAKE) -C dsp all

help:
	@$(ECHO) "make                  # build host and dsp executables"
	@$(ECHO) "make clean            # clean generated files"
	@$(ECHO) "make install          # stage install folder"

ifeq (install,$(MAKECMDGOALS))
ifeq (,$(EXEC_DIR))
EXEC_DIR=$(CURDIR)/install
endif
override EXEC_DIR:=$(EXEC_DIR)/ex04_syslink_dsp
endif

SLAVELOADER_DIR=$(SYSLINK_INSTALL_DIR)/packages/ti/syslink/bin
SLAVELOADER_DEBUG:=$(SLAVELOADER_DIR)/TI816X/samples/slaveloader_debug
SLAVELOADER_RELEASE:=$(SLAVELOADER_DIR)/TI816X/samples/slaveloader_release

install:
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	@$(MKDIR) $(EXEC_DIR)/debug
	$(INSTALL) run.sh $(EXEC_DIR)/debug
	$(CP) $(SLAVELOADER_DEBUG) $(EXEC_DIR)/debug/slaveloader
	@$(MKDIR) $(EXEC_DIR)/release
	$(INSTALL) run.sh $(EXEC_DIR)/release
	$(CP) $(SLAVELOADER_RELEASE) $(EXEC_DIR)/release/slaveloader
	$(MAKE) -C host EXEC_DIR=$(EXEC_DIR) install
	$(MAKE) -C dsp EXEC_DIR=$(EXEC_DIR) install

clean::
	$(RMDIR) install
	$(MAKE) -C host clean
	$(MAKE) -C dsp clean

ifneq (,$(wildcard $(XDC_INSTALL_DIR)/bin/echo.exe))
    CP      = $(XDC_INSTALL_DIR)/bin/cp
    ECHO    = $(XDC_INSTALL_DIR)/bin/echo
    INSTALL = $(XDC_INSTALL_DIR)/bin/cp
    MKDIR   = $(XDC_INSTALL_DIR)/bin/mkdir -p
    RMDIR   = $(XDC_INSTALL_DIR)/bin/rm -rf
else
    CP      = cp
    ECHO    = echo
    INSTALL = install
    MKDIR   = mkdir -p
    RMDIR   = rm -rf
endif

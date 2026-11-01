#
# DSP server build.
#

EXBASE = ..
include ../products.mak

.DEFAULT_GOAL := all

srcs = main_dsp.c Server.c AudioAlgo.c dsp_algorithm.c dsp_spatial.c
objs = $(addprefix bin/$(PROFILE)/obj/,$(patsubst %.c,%.oe674,$(srcs)))
libs = configuro/linker.cmd

PKGPATH := $(SYSLINK_INSTALL_DIR)/packages
PKGPATH := $(PKGPATH)+$(BIOS_INSTALL_DIR)/packages
PKGPATH := $(PKGPATH)+$(IPC_INSTALL_DIR)/packages
PKGPATH := $(PKGPATH)+$(XDC_INSTALL_DIR)/packages

.PHONY: all clean install help

all: configuro/linker.cmd
	$(MAKE) PROFILE=debug server_dsp.x
	$(MAKE) PROFILE=release server_dsp.x

server_dsp.x: bin/$(PROFILE)/server_dsp.xe674

bin/$(PROFILE)/server_dsp.xe674: $(objs) $(libs)
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/$(PROFILE)/obj/%.oe674: %.h
bin/$(PROFILE)/obj/%.oe674: %.c
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) --output_file=$@ -fc $<

bin/$(PROFILE)/obj/Server.oe674: AudioAlgo.h ../shared/AppCommon.h ../shared/hrir_catalog.h
bin/$(PROFILE)/obj/AudioAlgo.oe674: AudioAlgo.h dsp_algorithm.h dsp_spatial.h ../shared/AppCommon.h ../shared/hrir_catalog.h
bin/$(PROFILE)/obj/dsp_algorithm.oe674: dsp_algorithm.h
bin/$(PROFILE)/obj/dsp_spatial.oe674: dsp_spatial.h \
		dsp_spatial_coefficients.inc dsp_spatial_fft_twiddles.inc ../shared/hrir_catalog.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SPATIAL_CFLAGS_$(PROFILE)) \
		--output_file=$@ -fc dsp_spatial.c

configuro/linker.cmd: Dsp.cfg ../shared/config.bld
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(XDC_INSTALL_DIR)/xs --xdcpath="$(subst +,;,$(PKGPATH))" \
            xdc.tools.configuro -o configuro \
            -t ti.targets.elf.C674 -c $(CGT_C674_ELF_INSTALL_DIR) \
            -p ti.platforms.evmTI816X:dsp -b ../shared/config.bld \
            -r release Dsp.cfg

install:
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	@$(MKDIR) $(EXEC_DIR)/debug
	$(CP) bin/debug/server_dsp.xe674 $(EXEC_DIR)/debug
	@$(MKDIR) $(EXEC_DIR)/release
	$(CP) bin/release/server_dsp.xe674 $(EXEC_DIR)/release

help:
	@$(ECHO) "make                  # build DSP executable"
	@$(ECHO) "make clean            # clean generated files"

clean::
	$(RMDIR) configuro bin

ifeq (install,$(MAKECMDGOALS))
ifeq (,$(EXEC_DIR))
$(error must specify EXEC_DIR)
endif
endif

CGTOOLS = $(CGT_C674_ELF_INSTALL_DIR)

CC = $(CGTOOLS)/bin/cl6x -c
AR = $(CGTOOLS)/bin/ar6x rq
LD = $(CGTOOLS)/bin/lnk6x --abi=eabi
ST = $(CGTOOLS)/bin/strip6x

CPPFLAGS =
CFLAGS = -qq -pdsw225 $(CCPROFILE_$(PROFILE)) -I. $(COMPILER_OPTS)

LDFLAGS = -w -q -c -m $(@D)/obj/$(@F).map
LDLIBS = -l $(CGTOOLS)/lib/rts6740_elf.lib

CCPROFILE_debug = -D_DEBUG_=1 --symdebug:dwarf
CCPROFILE_release = -O2
SPATIAL_CFLAGS_release = --opt_level=3 --opt_for_speed=5
COMPILER_OPTS = $(shell cat configuro/compiler.opt)

ifneq (,$(wildcard $(XDC_INSTALL_DIR)/bin/echo.exe))
    CP      = $(XDC_INSTALL_DIR)/bin/cp
    ECHO    = $(XDC_INSTALL_DIR)/bin/echo
    MKDIR   = $(XDC_INSTALL_DIR)/bin/mkdir -p
    RMDIR   = $(XDC_INSTALL_DIR)/bin/rm -rf
else
    CP      = cp
    ECHO    = echo
    MKDIR   = mkdir -p
    RMDIR   = rm -rf
endif

ifneq (clean,$(MAKECMDGOALS))
ifneq (,$(PROFILE))
ifeq (,$(wildcard bin/$(PROFILE)/obj))
    $(shell $(MKDIR) -p bin/$(PROFILE)/obj)
endif
endif
endif

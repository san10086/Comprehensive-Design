#
# ARM host build.
#

EXBASE = ..
include $(EXBASE)/products.mak

.DEFAULT_GOAL := all

PKGPATH := $(SYSLINK_INSTALL_DIR)/packages
PKGPATH := $(PKGPATH)+$(BIOS_INSTALL_DIR)/packages
PKGPATH := $(PKGPATH)+$(IPC_INSTALL_DIR)/packages

PLATFORM_CFLAGS ?= -DPLATFORM_DM8168
ifneq (,$(ARM_SYSROOT))
SYSROOT_FLAGS = --sysroot=$(ARM_SYSROOT)
SYSROOT_INCS = -I$(ARM_SYSROOT)/usr/include
ifneq (,$(wildcard $(ARM_LINUX_INC)))
SYSROOT_INCS += -I$(ARM_LINUX_INC)
endif
else
SYSROOT_FLAGS =
SYSROOT_INCS =
endif

AUDIO_DIR = audio

objs = bin/$(PROFILE)/obj/main.ov5T \
       bin/$(PROFILE)/obj/syslinkDsp.ov5T \
       bin/$(PROFILE)/obj/audioConfig.ov5T \
       bin/$(PROFILE)/obj/audioCapture.ov5T \
       bin/$(PROFILE)/obj/audioPlayback.ov5T \
       bin/$(PROFILE)/obj/audioWav.ov5T
libs = $(SYSLINK_INSTALL_DIR)/packages/ti/syslink/lib/syslink.a_$(PROFILE)

.PHONY: all clean install help

all:
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(MAKE) PROFILE=debug app_host
	$(MAKE) PROFILE=release app_host

help:
	@$(ECHO) "make                  # build host executable"
	@$(ECHO) "make clean            # clean generated files"
	@$(ECHO) "app_host DSP smoke    # SysLink smoke test"
	@$(ECHO) "app_host DSP audio 5 [output.wav] [passthrough|swap|gain2|gain3|gain5|spatial40|spatial60|orbit|dual4060]  # threaded ALSA -> DSP -> playback, optionally save DSP output"
	@$(ECHO) "app_host DSP record output.wav 5  # ALSA capture -> WAV"
	@$(ECHO) "app_host DSP file input.wav [output.wav] [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]  # WAV file -> DSP -> playback"

install:
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	@$(MKDIR) $(EXEC_DIR)/debug
	$(CP) bin/debug/app_host $(EXEC_DIR)/debug
	@$(MKDIR) $(EXEC_DIR)/release
	$(CP) bin/release/app_host $(EXEC_DIR)/release

clean::
	$(RMDIR) bin

app_host: bin/$(PROFILE)/app_host

bin/$(PROFILE)/app_host: $(objs) $(libs)
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/$(PROFILE)/obj/main.ov5T: app/main.c ../shared/AppCommon.h ../shared/hrir_catalog.h \
        syslink/syslinkDsp.h $(AUDIO_DIR)/audioConfig.h \
        $(AUDIO_DIR)/audioCapture.h $(AUDIO_DIR)/audioPlayback.h \
        $(AUDIO_DIR)/audioWav.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

bin/$(PROFILE)/obj/syslinkDsp.ov5T: syslink/syslinkDsp.c syslink/syslinkDsp.h \
        ../shared/AppCommon.h ../shared/hrir_catalog.h ../shared/SystemCfg.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

bin/$(PROFILE)/obj/audioConfig.ov5T: $(AUDIO_DIR)/audioConfig.c \
        $(AUDIO_DIR)/audioConfig.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

bin/$(PROFILE)/obj/audioCapture.ov5T: $(AUDIO_DIR)/audioCapture.c \
        $(AUDIO_DIR)/audioCapture.h $(AUDIO_DIR)/audioConfig.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

bin/$(PROFILE)/obj/audioPlayback.ov5T: $(AUDIO_DIR)/audioPlayback.c \
        $(AUDIO_DIR)/audioPlayback.h $(AUDIO_DIR)/audioConfig.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

bin/$(PROFILE)/obj/audioWav.ov5T: $(AUDIO_DIR)/audioWav.c \
        $(AUDIO_DIR)/audioWav.h
	@$(ECHO) "#"
	@$(ECHO) "# Making $@ ..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

ifeq (install,$(MAKECMDGOALS))
ifeq (,$(EXEC_DIR))
$(error must specify EXEC_DIR)
endif
endif

CC = $(CGT_ARM_PREFIX)gcc -c -MD -MF $@.dep -march=armv5t
AR = $(CGT_ARM_PREFIX)ar cr
LD = $(CGT_ARM_PREFIX)gcc

CPPFLAGS = -D_REENTRANT -Dxdc_target_name__=GCArmv5T \
    -Dxdc_target_types__=gnu/targets/arm/std.h

CFLAGS = $(SYSROOT_FLAGS) -Wall -ffloat-store -fPIC -Wunused -Dfar= \
    $(CCPROFILE_$(PROFILE)) $(PLATFORM_CFLAGS) -I. -Iapp -Isyslink \
    -I$(AUDIO_DIR) -I../shared $(SYSROOT_INCS) \
    $(addprefix -I,$(subst +, ,$(PKGPATH)))

LDFLAGS = $(SYSROOT_FLAGS) $(LDPROFILE_$(PROFILE)) -Wall -Wl,-Map=$@.map
LDLIBS = -lasound -lpthread -lc -lrt -lm

CCPROFILE_debug = -ggdb -D DEBUG
CCPROFILE_release = -O3 -D NDEBUG

LDPROFILE_debug = -ggdb
LDPROFILE_release = -O3

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

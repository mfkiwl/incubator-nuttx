############################################################################
# arch/sim/src/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(TOPDIR)/tools/Make.defs

ARCH_SRCDIR = $(TOPDIR)$(DELIM)arch$(DELIM)$(CONFIG_ARCH)$(DELIM)src
APPSDIR     = $(TOPDIR)$(DELIM)$(CONFIG_APPS_DIR)

INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)
INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)chip
INCLUDES += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)sched

ifeq ($(CONFIG_VIDEOUTILS_OPENH264),y)
INCLUDES += ${INCDIR_PREFIX}$(APPSDIR)/videoutils/openh264/openh264/codec/api
endif

ifeq ($(CONFIG_VIDEOUTILS_LIBX264),y)
INCLUDES += ${INCDIR_PREFIX}$(APPSDIR)/videoutils/x264/x264
endif

ifeq ($(CONFIG_VIDEOUTILS_LIBMAD),y)
INCLUDES += ${INCDIR_PREFIX}$(APPSDIR)/audioutils/libmad/libmad
endif

ifeq ($(CONFIG_VIDEOUTILS_LAME),y)
INCLUDES += ${INCDIR_PREFIX}$(APPSDIR)/audioutils/lame/lame
endif

CPPFLAGS += $(INCLUDES)
CFLAGS += $(INCLUDES)
CXXFLAGS += $(INCLUDES)
AFLAGS += $(INCLUDES)

NUTTX = $(call CONVERT_PATH,$(TOPDIR)$(DELIM)nuttx$(EXEEXT))

# Determine which objects are required in the link.The
# sim_head object normally draws in all that is needed, but
# there are a fews that must be included because they
# are called only from the host OS-specific logic(HOSTOBJS)

LINKOBJS = sim_head$(OBJEXT)
REQUIREDOBJS = $(LINKOBJS) sim_doirq$(OBJEXT)
ifeq ($(CONFIG_SIM_NETUSRSOCK),y)
  REQUIREDOBJS += sim_usrsock$(OBJEXT)
endif

ifeq ($(CONFIG_HOST_X86_64),y)
ifeq ($(CONFIG_SIM_M32),y)
  ASRCS += sim_fork_x86.S
else
  ASRCS += sim_fork_x86_64.S
endif
else ifeq ($(CONFIG_HOST_X86),y)
  ASRCS += sim_fork_x86.S
else ifeq ($(CONFIG_HOST_ARM),y)
  ASRCS += sim_fork_arm.S
else ifeq ($(CONFIG_HOST_ARM64),y)
  ASRCS += sim_fork_arm64.S
endif

AOBJS = $(ASRCS:.S=$(OBJEXT))

CSRCS  = sim_initialize.c sim_idle.c sim_doirq.c sim_initialstate.c
CSRCS += sim_createstack.c sim_usestack.c sim_releasestack.c sim_stackframe.c
CSRCS += sim_exit.c sim_schedulesigaction.c sim_switchcontext.c sim_heap.c
CSRCS += sim_uart.c sim_copyfullstate.c sim_sigdeliver.c sim_tcbinfo.c sim_cpuinfo.c
CSRCS += sim_registerdump.c sim_saveusercontext.c sim_sectionheap.c
CSRCS += sim_checkhostfstypes.c

ifeq ($(CONFIG_SCHED_BACKTRACE),y)
CSRCS += sim_backtrace.c
endif

ifeq ($(CONFIG_ARCH_HAVE_FORK),y)
CSRCS += sim_fork.c
endif

VPATH = :sim
ifeq ($(CONFIG_WINDOWS_NATIVE),y)
  VPATH += :sim/win
else
  VPATH += :sim/posix
endif
DEPPATH = $(patsubst %,--dep-path %,$(subst :, ,$(VPATH)))

CFLAGS += -fvisibility=default
HOSTCFLAGS = $(ARCHWARNINGS) $(ARCHOPTIMIZATION) \
   $(ARCHCFLAGS) $(HOSTINCLUDES) $(EXTRAFLAGS) -D__SIM__ \
   -fvisibility=default

HOSTCFLAGS += ${INCDIR_PREFIX}$(ARCH_SRCDIR)
HOSTCFLAGS += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)chip
ifeq ($(CONFIG_HOST_MACOS),y)
  HOSTCFLAGS += -Wno-deprecated-declarations
endif

ifeq ($(CONFIG_FS_LARGEFILE),y)
  HOSTCFLAGS += -D_FILE_OFFSET_BITS=64
endif

HOSTSRCS  = sim_hostirq.c sim_hostmemory.c sim_hostmisc.c sim_hosttime.c sim_hostuart.c
HOSTSRCS += sim_hostfs.c

hostfs.h: $(TOPDIR)/include/nuttx/fs/hostfs.h
	@echo "CP:  $<"
	$(Q) cp $< $@

sim_hostfs.c: hostfs.h

STDLIBS += -lpthread
ifeq ($(CONFIG_HOST_MACOS),y)
ifeq ($(CONFIG_HAVE_CXXINITIALIZE),y)
  # Note: sim_macho_init.c is not in CSRCS because it's picky about
  # the place in the object list for linking. Namely, its constructor
  # should be the first one in the executable.
  HEADSRC = sim_macho_init.c

  # sim_macho_init.c is not compatible with chained fixups.
  # cf. https://github.com/apache/nuttx/issues/15208
  ifeq ($(shell $(LD) -ld_classic -no_fixup_chains 2>&1 | grep "unknown option"),)
    LDLINKFLAGS += -ld_classic -no_fixup_chains
    LDFLAGS += -Wl,-ld_classic,-no_fixup_chains
  endif
endif
else
  STDLIBS += -lrt
endif

ifeq ($(CONFIG_LIBM_TOOLCHAIN),y)
  STDLIBS += -lm
endif

ifeq ($(CONFIG_COVERAGE_TOOLCHAIN),y)
  STDLIBS += -lgcov
endif

ifeq ($(CONFIG_STACK_COLORATION),y)
  CSRCS += sim_checkstack.c
endif

ifeq ($(CONFIG_SPINLOCK),y)
  HOSTSRCS += sim_testset.c
endif

ifeq ($(CONFIG_SMP),y)
  CSRCS += sim_smpsignal.c sim_cpuidlestack.c
endif

ifeq ($(CONFIG_ARCH_HAVE_MULTICPU),y)
  HOSTSRCS += sim_hostsmp.c
endif

ifeq ($(CONFIG_ONESHOT),y)
  CSRCS += sim_oneshot.c
endif

ifeq ($(CONFIG_RTC_DRIVER),y)
  CSRCS += sim_rtc.c
endif

ifeq ($(CONFIG_SIM_LCDDRIVER),y)
  CSRCS += sim_lcd.c
else ifeq ($(CONFIG_SIM_FRAMEBUFFER),y)
  CSRCS += sim_framebuffer.c
endif

ifeq ($(CONFIG_SIM_X11FB),y)
  HOSTSRCS += sim_x11framebuffer.c
ifeq ($(CONFIG_HOST_MACOS),y)
  STDLIBS += -L/opt/X11/lib
endif
  STDLIBS += -lX11 -lXext
ifeq ($(CONFIG_SIM_TOUCHSCREEN),y)
  CSRCS += sim_touchscreen.c
  HOSTSRCS += sim_x11eventloop.c
else ifeq ($(CONFIG_SIM_AJOYSTICK),y)
  CSRCS += sim_ajoystick.c
  HOSTSRCS += sim_x11eventloop.c
else ifeq ($(CONFIG_SIM_BUTTONS),y)
  HOSTSRCS += sim_x11eventloop.c
endif

ifeq ($(CONFIG_SIM_KEYBOARD),y)
  CSRCS += sim_keyboard.c
endif
endif

ifeq ($(CONFIG_FS_FAT),y)
  CSRCS += sim_blockdevice.c sim_deviceimage.c
  STDLIBS += -lz
endif

ifneq ($(CONFIG_SIM_WIFIDEV_NUMBER),0)
  CFLAGS += -DTOPDIR=\"$(TOPDIR)\"
endif

ifeq ($(CONFIG_SIM_NETDEV_TAP),y)
  CSRCS += sim_netdriver.c
ifneq ($(CONFIG_WINDOWS_CYGWIN),y)
  HOSTSRCS += sim_tapdev.c
else # CONFIG_WINDOWS_CYGWIN != y
  HOSTSRCS += sim_wpcap.c
  STDLIBS = /lib/w32api/libws2_32.a /lib/w32api/libiphlpapi.a
endif # CONFIG_WINDOWS_CYGWIN != y
else ifeq ($(CONFIG_SIM_NETDEV_VPNKIT),y)
  CSRCS += sim_netdriver.c
  HOSTSRCS += sim_vpnkit.c
  VPATH += :sim/posix/vpnkit
  HOSTSRCS += sim_protocol.c sim_negotiate.c
endif

ifeq ($(CONFIG_SIM_NETUSRSOCK),y)
  HOSTSRCS += sim_hostusrsock.c
  CSRCS += sim_usrsock.c
endif

ifeq ($(CONFIG_SIM_HCISOCKET),y)
  HOSTSRCS += sim_hosthcisocket.c
  CSRCS += sim_hcisocket.c
endif

ifeq ($(CONFIG_SIM_I2CBUS_LINUX),y)
  HOSTSRCS += sim_linuxi2c.c
endif

ifeq ($(CONFIG_SIM_SPI_LINUX),y)
  HOSTSRCS += sim_linuxspi.c
endif

ifeq ($(CONFIG_SIM_USB_DEV),y)
  CSRCS += sim_usbdev.c
ifeq ($(CONFIG_SIM_USB_RAW_GADGET),y)
  HOSTSRCS += sim_rawgadget.c
endif
endif

ifeq ($(CONFIG_SIM_USB_HOST),y)
  CSRCS += sim_usbhost.c
ifeq ($(CONFIG_SIM_LIBUSB),y)
  HOSTSRCS += sim_libusb.c
  STDLIBS += -lusb-1.0
endif
endif

ifeq ($(CONFIG_RPMSG_VIRTIO_LITE),y)
  CSRCS += sim_rpmsg_virtio.c
endif

ifeq ($(CONFIG_RPMSG_PORT_UART),y)
  CSRCS += sim_rpmsg_port_uart.c
endif

ifeq ($(CONFIG_RPTUN),y)
  CSRCS += sim_rptun.c
endif

ifeq ($(CONFIG_SIM_SOUND_ALSA),y)
  CSRCS += sim_alsa.c
  CSRCS += sim_offload.c
  STDLIBS += -lasound
endif

ifeq ($(CONFIG_SIM_CAMERA_V4L2),y)
  HOSTSRCS += sim_host_v4l2.c
  CSRCS += sim_camera.c
  STDLIBS += -lv4l2
endif

ifeq ($(CONFIG_SIM_VIDEO_DECODER),y)
  CSRCS += sim_decoder.c
  CSRCS += sim_openh264dec.c
endif

ifeq ($(CONFIG_SIM_VIDEO_ENCODER),y)
  CSRCS += sim_encoder.c
  CSRCS += sim_x264encoder.c
  CFLAGS += -D_STDINT_H
endif

COBJS = $(CSRCS:.c=$(OBJEXT))

NUTTXOBJS = $(AOBJS) $(COBJS)
HOSTOBJS = $(HOSTSRCS:.c=$(OBJEXT))
HEADOBJ = $(HEADSRC:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS) $(HOSTSRCS)
OBJS = $(AOBJS) $(COBJS) $(HOSTOBJS)

$(foreach lib,$(notdir $(wildcard $(APPDIR)$(DELIM)staging$(DELIM)*$(LIBEXT))), \
  $(foreach elib,$(EXTRA_LIBS), \
    $(if $(filter $(notdir $(elib)),$(lib)), \
      $(eval NAMEFULL_LIBS+=$(elib)), \
      $(if $(filter $(notdir $(elib)),$(patsubst lib%$(LIBEXT),-l%,$(lib))), \
        $(eval NAMESPEC_LIBS+=$(elib)) \
       ) \
     ) \
   ) \
 )

EXTRA_LIBS := $(filter-out $(NAMEFULL_LIBS) $(NAMESPEC_LIBS),$(EXTRA_LIBS))
EXTRA_LIBS += $(wildcard $(APPDIR)$(DELIM)staging$(DELIM)*$(LIBEXT))

# Override in Make.defs if linker is not 'ld'

ifneq ($(CONFIG_HOST_MACOS),y)
  ARCHSCRIPT += nuttx.ld
  LDSTARTGROUP ?= --start-group
  LDENDGROUP ?= --end-group
endif

# Additional rules for system call wrapper

ifeq ($(CONFIG_SCHED_INSTRUMENTATION_SYSCALL),y)
  EXTRALINKCMDS += @$(TOPDIR)/syscall/syscall_wraps.ldcmd
endif

LDFLAGS += $(addprefix -T,$(call CONVERT_PATH,$(ARCHSCRIPT)))

# Determine which NuttX libraries will need to be linked in
# Most are provided by LINKLIBS on the MAKE command line

RELLIBS = $(patsubst %.a,%,$(patsubst lib%,-l%,$(LINKLIBS)))
RELPATHS += -L"$(TOPDIR)/staging"

# Add the board-specific library and directory

RELPATHS += -L board
RELLIBS += -lboard

# Make targets begin here

all: sim_head$(OBJEXT) libarch$(LIBEXT)

.PHONY: export_startup clean distclean cleanrel depend board/libboard$(LIBEXT)

$(AOBJS): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

$(COBJS) $(LINKOBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

$(HOSTOBJS) $(HEADOBJ): %$(OBJEXT): %.c
	$(Q) $(ECHO_BEGIN)"CC:  $<"
	$(Q) "$(CC)" -c $(HOSTCFLAGS) $< -o $@
	$(Q) $(ECHO_END)

# The architecture-specific library

libarch$(LIBEXT): $(NUTTXOBJS)
	$(call ARCHIVE, $@, $(NUTTXOBJS))

# The "board"-specific library. Of course, there really are no boards in
# the simulation.  However, this is a good place to keep parts of the simulation
# that are not hardware-related.

board/libboard$(LIBEXT):
	$(Q) $(MAKE) -C board libboard$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

# A partially linked object containing only NuttX code (no interface to host OS)
# Change the names of most symbols that conflict with libc symbols.
# Generate the final NuttX binary by linking the host-specific objects with the NuttX
# specific objects (with munged names)

# C++ global objects are constructed before main get executed, but it isn't a good
# point for simulator because NuttX doesn't finish the kernel initialization yet.
# So we have to skip the standard facilities and do the construction by ourself.
# But how to achieve the goal?
# 1.Command linker generate the default script(-verbose)
# 2.Move input sections out of .init_array/.fini_array into new .sinit/.einit
#   output sections
# 3.Replace __init_array_start/__init_array_end with _sinit/_einit
# 4.Append __init_array_start = .; __init_array_end = .;
# Step 2 cheat the host there is no object to construct (glibc >= 2.34)
# Step 3 let nxtask_startup find objects need to construct
# Step 4 cheat the host there is no object to construct (glibc < 2.34)
# Note: the destructor can be fixed in the same way.

nuttx-names.dat: nuttx-names.in
	$(call PREPROCESS, nuttx-names.in, nuttx-names.dat)

# When multiple linking, these two additional linking objects will be included

ifeq ($(CONFIG_MM_KASAN_GLOBAL),y)
EXTRALD_OBJ += kasan_globals$(OBJEXT)
endif
ifeq ($(CONFIG_ALLSYMS),y)
EXTRALD_OBJ += allsyms$(OBJEXT)
endif

define LINK_ALLSYMS_KASAN
	$(if $(CONFIG_ALLSYMS), \
		$(if $(CONFIG_HOST_MACOS), \
			$(Q) $(TOPDIR)/tools/mkallsyms.sh noconst $(NUTTX) $(CROSSDEV) > allsyms.tmp, \
			$(Q) $(TOPDIR)/tools/mkallsyms.py $(NUTTX) allsyms.tmp --orderbyname $(CONFIG_SYMTAB_ORDEREDBYNAME)))
	$(if $(CONFIG_ALLSYMS), \
		$(Q) $(call COMPILE, allsyms.tmp, allsyms$(OBJEXT), -x c)
		$(Q) $(call DELFILE, allsyms.tmp))
	$(if $(CONFIG_MM_KASAN_GLOBAL),
	$(Q) $(TOPDIR)/tools/kasan_global.py -e $(NUTTX) -o kasan_globals.tmp -a $(CONFIG_MM_KASAN_GLOBAL_ALIGN)
	$(Q) $(call COMPILE, kasan_globals.tmp, kasan_globals$(OBJEXT) -fno-sanitize=kernel-address, -x c)
	$(Q) $(call DELFILE, kasan_globals.tmp))
	$(if $(CONFIG_HAVE_CXX),\
	$(Q) "$(CXX)" $(CFLAGS) $(LDFLAGS) -o $(NUTTX) \
	      $(HEADOBJ) nuttx.rel $(HOSTOBJS) $(STDLIBS) $(EXTRALD_OBJ),\
	$(Q) "$(CC)" $(CFLAGS) $(LDFLAGS) -o $(NUTTX) \
	      $(HEADOBJ) nuttx.rel $(HOSTOBJS) $(STDLIBS) $(EXTRALD_OBJ))
endef

# Note: Use objcopy for Linux because for some reasons visibility=hidden
# stuff doesn't work there as we expect.
# Note: _stext stuff is for CONFIG_CXX_INITIALIZE_SINIT, which in not
# necessary for macOS.
nuttx$(EXEEXT): libarch$(LIBEXT) board/libboard$(LIBEXT) $(HEADOBJ) $(LINKOBJS) $(HOSTOBJS) nuttx-names.dat
	$(Q) echo "LD:  nuttx$(EXEEXT)"
	$(Q) $(LD) -r $(LDLINKFLAGS) $(RELPATHS) $(EXTRA_LIBPATHS) $(EXTRALINKCMDS) \
	     -o nuttx.rel $(REQUIREDOBJS) $(LDSTARTGROUP) $(RELLIBS) $(EXTRA_LIBS) $(LDENDGROUP)
ifneq ($(CONFIG_HOST_MACOS),y)
	$(Q) $(OBJCOPY) --redefine-syms=nuttx-names.dat nuttx.rel
	$(Q) $(CC) $(CFLAGS) -Wl,-verbose 2>&1 | \
	     sed -e '/====/,/====/!d;//d' \
	         -e '/__executable_start/s/$$/PROVIDE(_stext = .);/' \
	         -e 's/^\(\s\+\)\(\.init_array\)/\1\2 : { }\n\1.sinit/g' \
	         -e 's/^\(\s\+\)\(\.fini_array\)/\1\2 : { }\n\1.einit/g' \
	         -e 's/__init_array_start/_sinit/g' -e 's/__init_array_end/_einit/g' \
	         -e 's/__fini_array_start/_sfini/g' -e 's/__fini_array_end/_efini/g' >nuttx.ld
	$(Q) echo "__init_array_start = .; __init_array_end = .; __fini_array_start = .; __fini_array_end = .;" >>nuttx.ld
endif
ifeq ($(CONFIG_MM_KASAN_GLOBAL),y)
	$(Q) sed -i 's/\s*\.interp\s*:\s*{\s*\*(\.interp)\s*}/  \
	            .kasan.global : {KEEP(*(.data..LASAN0)) KEEP (*(.data.rel.local..LASAN0)) }\n  \
	            .interp : {*(.interp)}/g' nuttx.ld
endif
ifneq ($(CONFIG_SIM_CUSTOM_DATA_SECTION),"")
	$(Q) sed -i '/\.data *:/i $(subst ",, $(CONFIG_SIM_CUSTOM_DATA_SECTION))' nuttx.ld
endif
ifeq ($(CONFIG_ALLSYMS)$(CONFIG_MM_KASAN_GLOBAL),)
	$(if $(CONFIG_HAVE_CXX),\
	$(Q) "$(CXX)" $(CFLAGS) $(LDFLAGS) -o $(TOPDIR)/$@ $(HEADOBJ) nuttx.rel $(HOSTOBJS) $(STDLIBS),\
	$(Q) "$(CC)" $(CFLAGS) $(LDFLAGS) -o $(TOPDIR)/$@ $(HEADOBJ) nuttx.rel $(HOSTOBJS) $(STDLIBS))
else
	$(Q) $(call LINK_ALLSYMS_KASAN)
	$(Q) $(call LINK_ALLSYMS_KASAN)
	$(Q) $(call LINK_ALLSYMS_KASAN)
	$(Q) $(call LINK_ALLSYMS_KASAN)
endif
	$(Q) $(NM) $(TOPDIR)/$@ | \
		grep -v '\(compiled\)\|\(\.o$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | \
		sort > $(TOPDIR)/System.map

# This is part of the top-level export target

export_startup: sim_head.o $(HOSTOBJS) nuttx-names.dat
	cp sim_head.o $(HOSTOBJS) ${EXPORT_DIR}/startup
	cp nuttx-names.dat ${EXPORT_DIR}/libs

# Dependencies

makedepfile: $(CSRCS:.c=.ddc) $(ASRCS:.S=.dds) $(HOSTSRCS:.c=.ddh)
	$(call CATFILE, Make.dep, $^)
	$(call DELFILE, $^)

config.h: $(TOPDIR)/include/nuttx/config.h
	@echo "CP:  $<"
	$(Q) cp $< $@

.depend: Makefile config.h $(SRCS) $(TOPDIR)$(DELIM).config
	$(Q) if [ -e board/Makefile ]; then \
		$(MAKE) -C board depend ; \
	fi
	$(Q) $(MAKE) makedepfile
	$(Q) touch $@

depend: .depend

context::

clean:
	$(Q) if [ -e board/Makefile ]; then \
		$(MAKE) -C board clean ; \
	fi
	$(call DELFILE, nuttx.ld)
	$(call DELFILE, nuttx.rel)
	$(call DELFILE, nuttx-names.dat)
	$(call DELFILE, libarch$(LIBEXT))
	$(call CLEAN)

distclean:: clean
	$(Q) if [ -e board/Makefile ]; then \
		$(MAKE) -C board distclean ; \
	fi
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)
	$(call DELFILE, config.h)
	$(call DELFILE, hostfs.h)

-include Make.dep

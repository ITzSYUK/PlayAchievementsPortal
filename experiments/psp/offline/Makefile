.DEFAULT_GOAL := all
include ../../rcheevos.mk

RC_ROOT = ../../../vendor/rcheevos
# Editor-only validator assumes uint32_t is unsigned int; PSP newlib uses
# unsigned long. Neither editor API nor that validator is used by rc_client.
RC_SOURCES := $(filter-out $(RC_ROOT)/src/rcheevos/rc_validate.c $(RC_ROOT)/src/rapi/rc_api_editor.c,$(RC_SOURCES))
TARGET = RAOfflineProbe
VPATH = $(RC_ROOT)/src:$(RC_ROOT)/src/rcheevos:$(RC_ROOT)/src/rapi:$(RC_ROOT)/src/rhash:../../../online/src:../../common
OBJS = main.o offline_probe.o ra_psp_memory.o ra_psp_hash.o $(notdir $(RC_SOURCES:.c=.o))
INCDIR = $(RC_ROOT)/include $(RC_ROOT)/src/rhash ../../../online/include ../../common
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -D_GNU_SOURCE -include strings.h $(RC_DEFINES)
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lm
LDFLAGS =
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RetroAchievements Offline Probe
rc_client.o: CFLAGS += -Wno-unused-parameter
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

$(OBJS): Makefile ../../rcheevos.mk

TARGET = RAHttpsProbe
OBJS = main.o probe_log.o
VPATH = ../../common
INCDIR = ../../common
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
# build.mak appends net/apctl; GCC specs append utility/inet/resolver after libc.
# Repeating those archives here splits import tables around libc's references.
LIBS = -lpsphttp -lpspssl
LDFLAGS =
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RetroAchievements HTTPS Probe 0.2
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

$(OBJS): Makefile
main.o probe_log.o: ../../common/probe_log.h
$(PSP_EBOOT_SFO): Makefile

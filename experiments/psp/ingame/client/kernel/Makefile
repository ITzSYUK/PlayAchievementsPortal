TARGET = RAClientBootstrap
OBJS = main.o probe_log.o kernel_format.o placement2_policy.o
VPATH = ../../../../common ../../../../ingame
INCDIR = ../../../../common ../../../../ingame
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS =
BUILD_PRX = 1
USE_KERNEL_LIBS = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
.DEFAULT_GOAL := all
include $(PSPSDK)/lib/build.mak
$(OBJS): Makefile ../../../../ingame/client_shared.h ../../../../ingame/placement2_policy.h
probe_log.o: ../../../../common/probe_log.c
	$(CC) $(CFLAGS) -Dvsnprintf=ra_kernel_vsnprintf -c $< -o $@

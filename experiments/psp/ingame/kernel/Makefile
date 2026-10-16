TARGET = RAInGameProbe
OBJS = main.o probe_log.o probe_policy.o ra_psp_memory.o kernel_format.o
VPATH = ../../../common ../../../ingame ../../../../online/src
INCDIR = ../../../common ../../../ingame ../../../../online/include
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lgcc
BUILD_PRX = 1
USE_KERNEL_LIBS = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
$(OBJS): Makefile ../../../common/probe_log.h ../../../ingame/probe_policy.h ../../../ingame/probe_shared.h
probe_log.o: ../../../common/probe_log.c
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -Dvsnprintf=ra_kernel_vsnprintf -c $< -o $@
kernel_format.o: ../../../ingame/kernel_format.h

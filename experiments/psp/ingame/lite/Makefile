TARGET = RAInGameLite
OBJS = main.o probe_log.o kernel_format.o extra_ram_policy.o placement2_policy.o
VPATH = ../../../common ../../../ingame
INCDIR = ../../../common ../../../ingame
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS =
BUILD_PRX = 1
USE_KERNEL_LIBS = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
$(OBJS): Makefile ../../../common/probe_log.h ../../../ingame/kernel_format.h
main.o extra_ram_policy.o: ../../../ingame/extra_ram_policy.h
main.o placement2_policy.o: ../../../ingame/placement2_policy.h
placement2_policy.o: ../../../ingame/placement2_policy.c
probe_log.o: ../../../common/probe_log.c
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -Dvsnprintf=ra_kernel_vsnprintf -c $< -o $@

TARGET = RAClientUser
RC_ROOT = ../../../../../vendor/rcheevos
RC_SOURCES = $(RC_ROOT)/src/rc_client.c $(RC_ROOT)/src/rc_compat.c $(RC_ROOT)/src/rc_util.c $(RC_ROOT)/src/rc_version.c $(filter-out $(RC_ROOT)/src/rcheevos/rc_validate.c,$(wildcard $(RC_ROOT)/src/rcheevos/*.c)) $(filter-out $(RC_ROOT)/src/rapi/rc_api_editor.c,$(wildcard $(RC_ROOT)/src/rapi/*.c))
OBJS = main.o ra_client_session.o probe_log.o kernel_format.o $(notdir $(RC_SOURCES:.c=.o))
OBJS += md5.o
VPATH = ../../../../../online/src ../../../../common ../../../../ingame $(RC_ROOT)/src $(RC_ROOT)/src/rcheevos $(RC_ROOT)/src/rapi $(RC_ROOT)/src/rhash
INCDIR = ../../../../../online/include ../../../../common ../../../../ingame $(RC_ROOT)/include
LIBDIR =
CFLAGS = -Os -G0 -std=c99 -Wall -Wextra -Werror -D_GNU_SOURCE -DRC_NO_THREADS
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
rc_client.o: CFLAGS += -Wno-unused-parameter
$(notdir $(RC_SOURCES:.c=.o)): CFLAGS += -include strings.h
# Pinned code uses %u/%d for uint32_t/int32_t. PSP newlib typedefs these as
# long, but both int and long have the same 32-bit ABI. Keep our code strict.
format.o runtime.o rc_api_common.o rc_api_info.o rc_api_runtime.o: CFLAGS += -Wno-format
LIBS = -lm
LDFLAGS = -nostartfiles -Wl,-e,module_start
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
.DEFAULT_GOAL := all
include $(PSPSDK)/lib/build.mak
$(OBJS): Makefile ../../../../ingame/client_shared.h ../../../../../online/include/ra_client_session.h
probe_log.o: ../../../../common/probe_log.c ../../../../common/probe_log.h ../../../../ingame/kernel_format.h
	$(CC) $(CFLAGS) -Dvsnprintf=ra_kernel_vsnprintf -c $< -o $@

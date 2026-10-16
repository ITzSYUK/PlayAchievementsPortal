TARGET = RAInGameWorker
TLS_ROOT = ../../../../vendor/mbedtls
TLS_CONFIG_DIR = ../../../tls
include ../../../tls/mbedtls.mk
TLS_OBJS = $(patsubst $(TLS_ROOT)/library/%.c,objects/%.o,$(TLS_SOURCES))
OBJS = main.o kubridge.o probe_log.o tls_probe.o rtc_epoch.o probe_policy.o probe_power.o md5.o $(TLS_OBJS)
VPATH = ../../../common ../../../tls ../../../ingame ../../mbedtls ../../../../vendor/rcheevos/src/rhash
INCDIR = ../../../common ../../../tls ../../../ingame ../../entropy ../../../../vendor/mbedtls/include ../../../../vendor/mbedtls/library ../../../../vendor/rcheevos/src/rhash
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L -DMBEDTLS_CONFIG_FILE=\"psp_tls_config.h\"
CXXFLAGS = $(CFLAGS)
ASFLAGS = -G0
LIBS = -lpsppower
# Custom module_start/stop init libc in the worker, no CRT exit/ExitGame.
LDFLAGS = -nostartfiles -Wl,-e,module_start
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
.DEFAULT_GOAL := all
include $(PSPSDK)/lib/build.mak
objects/%.o: $(TLS_ROOT)/library/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -MMD -MP -c $< -o $@
$(OBJS): Makefile ../../../tls/psp_tls_config.h ../../../ingame/probe_shared.h
main.o probe_policy.o: ../../../ingame/probe_policy.h
main.o probe_power.o: ../../../ingame/probe_power.h
main.o: ../../entropy/entropy_request.h ../../../tls/tls_probe.h ../../../tls/rtc_epoch.h
tls_probe.o: ../../../tls/tls_probe.h
-include $(TLS_OBJS:.o=.d)

TARGET = RAMbedTLSProbe
TLS_ROOT = ../../../vendor/mbedtls
TLS_CONFIG_DIR = ../../tls
include ../../tls/mbedtls.mk
TLS_OBJS = $(patsubst $(TLS_ROOT)/library/%.c,objects/%.o,$(TLS_SOURCES))
OBJS = main.o kubridge.o probe_log.o tls_probe.o rtc_epoch.o $(TLS_OBJS)
VPATH = ../../common ../../tls
INCDIR = ../../common ../../tls ../../../vendor/mbedtls/include ../../../vendor/mbedtls/library ../entropy
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L -DMBEDTLS_CONFIG_FILE=\"psp_tls_config.h\"
CXXFLAGS = $(CFLAGS)
ASFLAGS = -G0
LIBS =
LDFLAGS =
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RetroAchievements Mbed TLS Probe 0.2
PSPSDK := $(shell psp-config --pspsdk-path)
.DEFAULT_GOAL := all
include $(PSPSDK)/lib/build.mak
objects/%.o: $(TLS_ROOT)/library/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -MMD -MP -c $< -o $@
$(OBJS): Makefile ../../tls/psp_tls_config.h
main.o: ../entropy/entropy_request.h
main.o rtc_epoch.o: ../../tls/rtc_epoch.h
tls_probe.o main.o: ../../tls/tls_probe.h ../../common/probe_log.h
probe_log.o: ../../common/probe_log.h
$(PSP_EBOOT_SFO): Makefile
-include $(TLS_OBJS:.o=.d)

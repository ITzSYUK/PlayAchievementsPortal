include mbedtls.mk
TLS_CONFIG_DIR = .
BUILD ?= ../../build/tls-host
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Werror
CPPFLAGS += $(TLS_CPPFLAGS) -I../common -D_POSIX_C_SOURCE=200809L
OBJECTS = $(patsubst $(TLS_ROOT)/library/%.c,$(BUILD)/mbedtls/%.o,$(TLS_SOURCES))
.PHONY: all test
all: $(BUILD)/tls_probe $(BUILD)/test_rtc_epoch
$(BUILD)/mbedtls/%.o: $(TLS_ROOT)/library/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/probe_log.o: ../common/probe_log.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/libmbedtls.a: $(OBJECTS)
	$(AR) rcs $@ $^
$(BUILD)/tls_probe: $(BUILD)/host_main.o $(BUILD)/tls_probe.o $(BUILD)/probe_log.o $(BUILD)/libmbedtls.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
$(BUILD)/test_rtc_epoch: $(BUILD)/test_rtc_epoch.o $(BUILD)/rtc_epoch.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
test: all
	$(BUILD)/test_rtc_epoch
	python3 test_tls.py $(BUILD)/tls_probe
-include $(OBJECTS:.o=.d) $(BUILD)/host_main.d $(BUILD)/tls_probe.d $(BUILD)/probe_log.d
-include $(BUILD)/test_rtc_epoch.d $(BUILD)/rtc_epoch.d
$(OBJECTS) $(BUILD)/host_main.o $(BUILD)/tls_probe.o: Makefile mbedtls.mk psp_tls_config.h tls_probe.h

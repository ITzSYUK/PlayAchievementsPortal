TLS_ROOT = ../../../vendor/mbedtls
include ../mbedtls.mk
BUILD ?= ../../../build/compact-tls-host
CC ?= cc
CFLAGS ?= -Os -g -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
CPPFLAGS = -I../../../online/include -I../../ingame -I.. -I$(TLS_ROOT)/include -I$(TLS_ROOT)/library -DMBEDTLS_CONFIG_FILE=\"compact_tls_config.h\" -D_POSIX_C_SOURCE=200809L -Dsnprintf=ra_kernel_snprintf
OBJECTS = $(patsubst $(TLS_ROOT)/library/%.c,$(BUILD)/tls/%.o,$(TLS_SOURCES))
.PHONY: test all
all: $(BUILD)/https $(BUILD)/test_http $(BUILD)/test_resources
$(BUILD)/tls/%.o: $(TLS_ROOT)/library/%.c ../compact_tls_config.h ../psp_tls_config.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/https: host_main.c ../../../online/src/ra_https.c ../../../online/src/ra_http_body.c ../../ingame/kernel_format.c $(OBJECTS) ../../../online/include/ra_https.h ../../../online/include/ra_http_body.h ../../../online/include/ra_response_limits.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.o,$^) $(LDFLAGS) -Wl,--gc-sections -o $@
$(BUILD)/test_http: test_http.c ../../../online/src/ra_http_body.c ../../ingame/kernel_format.c ../../../online/include/ra_http_body.h ../../../online/include/ra_response_limits.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_http.c ../../../online/src/ra_http_body.c ../../ingame/kernel_format.c $(LDFLAGS) -o $@
test: all
	$(BUILD)/test_http
	$(BUILD)/test_resources
	python3 test_https.py $(BUILD)/https
$(BUILD)/test_resources: test_resources.c ../../../online/src/ra_net_resources.c ../../../online/include/ra_net_resources.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_resources.c ../../../online/src/ra_net_resources.c $(LDFLAGS) -o $@

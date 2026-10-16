TLS_ROOT ?= ../../vendor/mbedtls
TLS_CONFIG_DIR ?= ../tls
TLS_CPPFLAGS = -I$(TLS_ROOT)/include -I$(TLS_ROOT)/library -I$(TLS_CONFIG_DIR) -DMBEDTLS_CONFIG_FILE=\"psp_tls_config.h\"
# Unmodified upstream translation units; disabled modules compile empty.
TLS_SOURCES = $(wildcard $(TLS_ROOT)/library/*.c)

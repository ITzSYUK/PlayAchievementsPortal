#ifndef RA_COMPACT_TLS_CONFIG_H
#define RA_COMPACT_TLS_CONFIG_H
#include "psp_tls_config.h"
/* Dedicated RA endpoint experiment, ECDHE/ECDSA only. Unsupported chains
 * fail closed; no change to the existing standalone probe configuration. */
#undef MBEDTLS_RSA_C
#undef MBEDTLS_PKCS1_V15
#undef MBEDTLS_PKCS1_V21
#undef MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#undef MBEDTLS_CHACHA20_C
#undef MBEDTLS_POLY1305_C
#undef MBEDTLS_CHACHAPOLY_C
#undef MBEDTLS_PEM_PARSE_C
#undef MBEDTLS_BASE64_C
#undef MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 1024
#define MBEDTLS_ECP_WINDOW_SIZE 2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0
#endif

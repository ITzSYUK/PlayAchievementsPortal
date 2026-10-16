#ifndef RA_KERNEL_FORMAT_H
#define RA_KERNEL_FORMAT_H
#include <stdarg.h>
#include <stddef.h>
/* Bounded integer/string subset used by the kernel diagnostic's logger. */
int ra_kernel_vsnprintf(char*, size_t, const char*, va_list);
int ra_kernel_snprintf(char*, size_t, const char*, ...);
#endif

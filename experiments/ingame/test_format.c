#include "kernel_format.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static int format(char* output, size_t capacity, const char* text, ...)
{
    int result; va_list args;
    va_start(args, text); result = ra_kernel_vsnprintf(output, capacity, text, args); va_end(args);
    return result;
}
int main(void)
{
    char output[128], reference[128], small[5] = {0};
    const char* pattern = "%08X %u %d %.4s %%";
    int expected = snprintf(reference, sizeof(reference), pattern, UINT_MAX, UINT_MAX, INT_MIN, "hello");
    if (format(output, sizeof(output), pattern, UINT_MAX, UINT_MAX, INT_MIN, "hello") != expected || strcmp(output, reference)) return 1;
    if (format(small, sizeof(small), "%s", "12345678") != 8 || strcmp(small, "1234")) return 1;
    if (format(NULL, 0, "%s", "abcdef") != 6 || format(output, sizeof(output), "%f", 1.0) != -1) return 1;
    puts("PASS: bounded kernel formatter, integer limits, precision and truncation");
    return 0;
}

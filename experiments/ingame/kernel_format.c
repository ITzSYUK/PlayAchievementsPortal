#include "kernel_format.h"
#include <limits.h>
int ra_kernel_snprintf(char* output, size_t capacity, const char* format, ...)
{
    va_list args;
    int rc;
    va_start(args, format); rc = ra_kernel_vsnprintf(output, capacity, format, args); va_end(args);
    return rc;
}
static void emit(char* output, size_t capacity, size_t* count, char ch)
{
    if (*count + 1 < capacity) output[*count] = ch;
    ++*count;
}
int ra_kernel_vsnprintf(char* output, size_t capacity, const char* format, va_list args)
{
    size_t count = 0;
    if (!format || (capacity && !output)) return -1;
    while (*format) {
        unsigned width = 0, precision = UINT_MAX;
        int zero = 0;
        char kind;
        if (*format != '%') { emit(output, capacity, &count, *format++); continue; }
        ++format;
        if (*format == '%') { emit(output, capacity, &count, *format++); continue; }
        if (*format == '0') { zero = 1; ++format; }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + *format++ - '0';
            if (width > 64) return -1;
        }
        if (*format == '.') {
            ++format; precision = 0;
            while (*format >= '0' && *format <= '9') {
                precision = precision * 10 + *format++ - '0';
                if (precision > 4096) return -1;
            }
        }
        kind = *format;
        if (!kind) return -1;
        ++format;
        if (kind == 's') {
            const char* string = va_arg(args, const char*);
            unsigned length = 0, i;
            if (!string) string = "(null)";
            while (length < precision && string[length]) ++length;
            for (i = length; i < width; ++i) emit(output, capacity, &count, ' ');
            for (i = 0; i < length; ++i) emit(output, capacity, &count, string[i]);
        } else if (kind == 'u' || kind == 'd' || kind == 'X' || kind == 'x') {
            char digits[32];
            const char* hex = kind == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            unsigned number, length = 0, total, i, base = kind == 'X' || kind == 'x' ? 16 : 10;
            int negative = 0;
            if (kind == 'd') {
                int value = va_arg(args, int);
                negative = value < 0; number = negative ? 0u - (unsigned)value : (unsigned)value;
            } else number = va_arg(args, unsigned);
            do { digits[length++] = hex[number % base]; number /= base; } while (number);
            total = length + (unsigned)negative;
            if (negative && zero) emit(output, capacity, &count, '-');
            for (i = total; i < width; ++i) emit(output, capacity, &count, zero ? '0' : ' ');
            if (negative && !zero) emit(output, capacity, &count, '-');
            while (length) emit(output, capacity, &count, digits[--length]);
        } else return -1;
        if (count > INT_MAX) return -1;
    }
    if (capacity) output[count < capacity ? count : capacity - 1] = 0;
    return count > INT_MAX ? -1 : (int)count;
}

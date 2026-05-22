#include "trap.h"
#include <stdarg.h>

#define TEST_MMIO_ADDR 0x1FFFF000u

static volatile u32 *const test_mmio =
    (volatile u32 *)TEST_MMIO_ADDR;

int strcmp(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *lhs == *rhs) {
        ++lhs;
        ++rhs;
    }
    return (int)(unsigned char)(*lhs) - (int)(unsigned char)(*rhs);
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++) != '\0') {
    }
    return ret;
}

char *strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst != '\0') {
        ++dst;
    }
    while ((*dst++ = *src++) != '\0') {
    }
    return ret;
}

void *memset(void *dst, int ch, size_t len) {
    unsigned char *p = (unsigned char *)dst;
    while (len-- != 0u) {
        *p++ = (unsigned char)ch;
    }
    return dst;
}

int memcmp(const void *lhs, const void *rhs, size_t len) {
    const unsigned char *a = (const unsigned char *)lhs;
    const unsigned char *b = (const unsigned char *)rhs;
    while (len-- != 0u) {
        if (*a != *b) {
            return (int)(*a) - (int)(*b);
        }
        ++a;
        ++b;
    }
    return 0;
}

static char *append_string(char *dst, const char *src) {
    while (*src != '\0') {
        *dst++ = *src++;
    }
    return dst;
}

static char *append_decimal(char *dst, int value) {
    char buffer[16];
    unsigned int magnitude;
    size_t len = 0;

    if (value < 0) {
        *dst++ = '-';
        magnitude = (unsigned int)(-(value + 1)) + 1u;
    } else {
        magnitude = (unsigned int)value;
    }

    do {
        buffer[len++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u);

    while (len-- != 0u) {
        *dst++ = buffer[len];
    }
    return dst;
}

typedef struct {
    uint32_t lo;
    uint32_t hi;
} u64_parts;

typedef union {
    long long signed_value;
    u64_parts parts;
} i64_cast;

static u64_parts u64_shl1(u64_parts value) {
    u64_parts shifted;
    shifted.hi = (value.hi << 1) | (value.lo >> 31);
    shifted.lo = value.lo << 1;
    return shifted;
}

static int u64_compare(u64_parts lhs, u64_parts rhs) {
    if (lhs.hi != rhs.hi) {
        return (lhs.hi > rhs.hi) ? 1 : -1;
    }
    if (lhs.lo != rhs.lo) {
        return (lhs.lo > rhs.lo) ? 1 : -1;
    }
    return 0;
}

static u64_parts u64_sub(u64_parts lhs, u64_parts rhs) {
    u64_parts result;
    result.lo = lhs.lo - rhs.lo;
    result.hi = lhs.hi - rhs.hi - (lhs.lo < rhs.lo ? 1u : 0u);
    return result;
}

static u64_parts u64_negate(u64_parts value) {
    value.lo = ~value.lo + 1u;
    value.hi = ~value.hi + (value.lo == 0u ? 1u : 0u);
    return value;
}

static u64_parts u64_unsigned_mod(u64_parts dividend, u64_parts divisor) {
    u64_parts remainder = {0u, 0u};
    int bit;

    if (divisor.lo == 0u && divisor.hi == 0u) {
        return remainder;
    }

    for (bit = 0; bit < 64; ++bit) {
        remainder = u64_shl1(remainder);
        if ((dividend.hi & 0x80000000u) != 0u) {
            remainder.lo |= 0x1u;
        }
        dividend = u64_shl1(dividend);
        if (u64_compare(remainder, divisor) >= 0) {
            remainder = u64_sub(remainder, divisor);
        }
    }

    return remainder;
}

long long __moddi3(long long lhs, long long rhs) {
    i64_cast dividend;
    i64_cast divisor;
    i64_cast result;
    int dividend_negative;

    dividend.signed_value = lhs;
    divisor.signed_value = rhs;
    dividend_negative = (dividend.parts.hi & 0x80000000u) != 0u;

    if ((dividend.parts.hi & 0x80000000u) != 0u) {
        dividend.parts = u64_negate(dividend.parts);
    }
    if ((divisor.parts.hi & 0x80000000u) != 0u) {
        divisor.parts = u64_negate(divisor.parts);
    }

    result.parts = u64_unsigned_mod(dividend.parts, divisor.parts);
    if (dividend_negative) {
        result.parts = u64_negate(result.parts);
    }
    return result.signed_value;
}

int sprintf(char *dst, const char *fmt, ...) {
    va_list ap;
    char *out = dst;

    va_start(ap, fmt);
    while (*fmt != '\0') {
        if (*fmt != '%') {
            *out++ = *fmt++;
            continue;
        }

        ++fmt;
        if (*fmt == '\0') {
            break;
        }

        if (*fmt == '%') {
            *out++ = '%';
        } else if (*fmt == 's') {
            out = append_string(out, va_arg(ap, const char *));
        } else if (*fmt == 'd') {
            out = append_decimal(out, va_arg(ap, int));
        } else {
            *out++ = '%';
            *out++ = *fmt;
        }
        ++fmt;
    }
    *out = '\0';
    va_end(ap);
    return (int)(out - dst);
}

void goodtrap(void) {
    *test_mmio = 0;
    while (1) { }
}

void badtrap(u32 code) {
    *test_mmio = code;
    while (1) { }
}

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef int            int32_t;
typedef unsigned int   size_t;
typedef uint32_t       u32;

#define LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

int strcmp(const char *lhs, const char *rhs);
char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
void *memset(void *dst, int ch, size_t len);
int memcmp(const void *lhs, const void *rhs, size_t len);
int sprintf(char *dst, const char *fmt, ...);

void goodtrap(void);
void badtrap(u32 code);

static inline void check(int cond) {
    if (!cond) {
        badtrap(1);
    }
}

#ifdef __cplusplus
}
#endif

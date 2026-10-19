#ifndef TEST_UTIL_H
#define TEST_UTIL_H

/* Minimal assertion helpers for the host tests. Every test program ends with
 * TEST_FINISH(), which prints a RESULT line that host_test.py collects. */

#include <stdio.h>

static int g_testPassed;
static int g_testFailed;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) {                                                          \
            ++g_testPassed;                                                  \
        } else {                                                             \
            ++g_testFailed;                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* 64-bit safe: "long" is 32 bits on some host compilers. */
#define CHECK_EQ_U(actual, expected)                                         \
    do {                                                                     \
        unsigned long long a_ = (unsigned long long)(actual);                \
        unsigned long long e_ = (unsigned long long)(expected);              \
        if (a_ == e_) {                                                      \
            ++g_testPassed;                                                  \
        } else {                                                             \
            ++g_testFailed;                                                  \
            printf("FAIL %s:%d: %s == %llu, expected %llu\n", __FILE__,      \
                   __LINE__, #actual, a_, e_);                               \
        }                                                                    \
    } while (0)

/* Compile-time check usable at file scope (C99, no static assert). */
#define STATIC_CHECK(name, cond) typedef char static_check_##name[(cond) ? 1 : -1]

#define TEST_FINISH()                                                        \
    do {                                                                     \
        printf("RESULT passed=%d failed=%d\n", g_testPassed, g_testFailed);  \
        return (g_testFailed == 0) ? 0 : 1;                                  \
    } while (0)

#endif /* TEST_UTIL_H */

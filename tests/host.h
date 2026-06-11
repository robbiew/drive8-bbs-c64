/* Minimal host-side test harness shared by tests/test_*.c. */
#ifndef TESTS_HOST_H
#define TESTS_HOST_H
#include <stdio.h>
#include <string.h>

static int t_fails = 0;
static int t_count = 0;

#define EXPECT_EQ(name, got, want) do { \
    long g_ = (long)(got), w_ = (long)(want); t_count++; \
    if (g_ != w_) { printf("FAIL %s: got %ld want %ld\n", (name), g_, w_); t_fails++; } \
} while (0)

#define EXPECT_STR(name, got, want) do { \
    t_count++; \
    if (strcmp((got), (want)) != 0) { \
        printf("FAIL %s: got \"%s\" want \"%s\"\n", (name), (got), (want)); t_fails++; } \
} while (0)

#define EXPECT_MEM(name, got, want, n) do { \
    t_count++; \
    if (memcmp((got), (want), (n)) != 0) { printf("FAIL %s\n", (name)); t_fails++; } \
} while (0)

static int test_summary(const char *suite) {
    printf("%s: %d checks, %d failed\n", suite, t_count, t_fails);
    return t_fails ? 1 : 0;
}
#endif

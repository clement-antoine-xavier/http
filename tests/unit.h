#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/**
 * @file unit.h
 * @brief Minimal zero-dependency unit test harness.
 *
 * A test is declared with the TEST() macro. Each test self-registers through a
 * compiler constructor, so tests/main (unit.c) only has to walk the registry.
 * Assertions come in two flavours:
 *
 *   - CHECK_*   record a failure and keep running the test.
 *   - REQUIRE_* record a failure and return from the test immediately.
 *
 * Registration relies on the GCC/Clang `constructor` attribute, which is
 * available on every compiler this project targets.
 */

/** @brief Maximum number of registered tests. */
#define UNIT_MAX_TESTS 256

/** @brief A registered test: a display name and its function. */
typedef struct
{
    const char *name;
    void (*fn)(void);
} unit_test_t;

extern unit_test_t unit_tests[UNIT_MAX_TESTS];
extern size_t unit_test_count;
extern int unit_failures;

/**
 * @brief Register a test; called automatically by TEST().
 */
void unit_register(const char *name, void (*fn)(void));

/**
 * @brief Report an assertion failure with the source location.
 */
void unit_fail(const char *file, int line, const char *fmt, ...);

/**
 * @brief Declare a test.
 *
 * @code
 * TEST(set_and_get)
 * {
 *     ...
 * }
 * @endcode
 */
#define TEST(name)                                                             \
    static void name(void);                                                    \
    __attribute__((constructor)) static void unit_register_##name(void)        \
    {                                                                          \
        unit_register(#name, name);                                            \
    }                                                                          \
    static void name(void)

/** @brief Fail the current test if @p cond is false, then continue. */
#define CHECK(cond)                                                            \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
            unit_fail(__FILE__, __LINE__, "CHECK(%s) failed", #cond);          \
    } while (0)

/** @brief Fail the current test if @p cond is false, then return. */
#define REQUIRE(cond)                                                          \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            unit_fail(__FILE__, __LINE__, "REQUIRE(%s) failed", #cond);        \
            return;                                                            \
        }                                                                      \
    } while (0)

/** @brief Fail unless @p a and @p b are equal integers. */
#define CHECK_INT_EQ(a, b)                                                     \
    do                                                                         \
    {                                                                          \
        long long unit_a = (long long)(a);                                     \
        long long unit_b = (long long)(b);                                     \
        if (unit_a != unit_b)                                                  \
            unit_fail(__FILE__, __LINE__, "expected %lld == %lld",             \
                      unit_a, unit_b);                                         \
    } while (0)

/** @brief Fail unless @p a and @p b are equal strings (both non-NULL). */
#define CHECK_STR_EQ(a, b)                                                     \
    do                                                                         \
    {                                                                          \
        const char *unit_a = (a);                                              \
        const char *unit_b = (b);                                              \
        if (unit_a == NULL || unit_b == NULL || strcmp(unit_a, unit_b) != 0)   \
            unit_fail(__FILE__, __LINE__, "expected \"%s\" == \"%s\"",         \
                      unit_a ? unit_a : "(null)",                             \
                      unit_b ? unit_b : "(null)");                            \
    } while (0)

/** @brief Fail unless @p p is NULL. */
#define CHECK_NULL(p) CHECK((p) == NULL)

/** @brief Fail unless @p p is non-NULL. */
#define CHECK_NOT_NULL(p) CHECK((p) != NULL)

/** @brief Fail unless @p p is truthy. */
#define CHECK_TRUE(p) CHECK((p) != 0)

/** @brief Fail unless @p p is falsy. */
#define CHECK_FALSE(p) CHECK((p) == 0)

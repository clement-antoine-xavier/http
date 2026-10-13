#include "unit.h"

#include <stdarg.h>

unit_test_t unit_tests[UNIT_MAX_TESTS];
size_t unit_test_count = 0;
int unit_failures = 0;

static int unit_current_failed = 0;

void unit_register(const char *name, void (*fn)(void))
{
    if (unit_test_count >= UNIT_MAX_TESTS)
    {
        fprintf(stderr, "unit: too many tests (max %d), dropping %s\n",
                UNIT_MAX_TESTS, name);
        return;
    }
    unit_tests[unit_test_count].name = name;
    unit_tests[unit_test_count].fn = fn;
    unit_test_count++;
}

void unit_fail(const char *file, int line, const char *fmt, ...)
{
    unit_current_failed = 1;
    fprintf(stderr, "  %s:%d: ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

int main(void)
{
    for (size_t i = 0; i < unit_test_count; i++)
    {
        unit_current_failed = 0;
        unit_tests[i].fn();
        if (unit_current_failed)
        {
            unit_failures++;
            printf("FAIL %s\n", unit_tests[i].name);
        }
        else
        {
            printf("PASS %s\n", unit_tests[i].name);
        }
    }

    printf("\n%zu test(s), %d failed\n", unit_test_count, unit_failures);
    return unit_failures == 0 ? 0 : 1;
}

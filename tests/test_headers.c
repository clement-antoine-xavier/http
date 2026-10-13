#include "unit.h"

#include "http/headers.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

#define CTX_MAX 16

typedef struct
{
    const char *names[CTX_MAX];
    const char *values[CTX_MAX];
    size_t count;
    int stop_after; /* 0 means never stop early */
} collect_ctx_t;

static int collect(const char *name, const char *value, void *ctx)
{
    collect_ctx_t *c = ctx;
    if (c->count < CTX_MAX)
    {
        c->names[c->count] = name;
        c->values[c->count] = value;
        c->count++;
    }
    if (c->stop_after && (int)c->count >= c->stop_after)
        return 42;
    return 0;
}

static int ctx_has_value(const collect_ctx_t *c, const char *value)
{
    for (size_t i = 0; i < c->count; i++)
        if (strcmp(c->values[i], value) == 0)
            return 1;
    return 0;
}

static int ctx_has_name(const collect_ctx_t *c, const char *name)
{
    for (size_t i = 0; i < c->count; i++)
        if (strcmp(c->names[i], name) == 0)
            return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

TEST(create_returns_empty_map)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_count(h), 0);
    destroy_headers(h);
}

TEST(destroy_null_is_safe)
{
    destroy_headers(NULL);
}

/* ------------------------------------------------------------------------- */
/* set / get                                                                 */
/* ------------------------------------------------------------------------- */

TEST(set_and_get)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_set(h, "Host", "example.com"), 0);
    CHECK_STR_EQ(headers_get(h, "Host"), "example.com");
    CHECK_INT_EQ(headers_count(h), 1);
    destroy_headers(h);
}

TEST(get_is_case_insensitive)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_set(h, "Content-Type", "text/html"), 0);
    CHECK_STR_EQ(headers_get(h, "content-type"), "text/html");
    CHECK_STR_EQ(headers_get(h, "CONTENT-TYPE"), "text/html");
    CHECK_STR_EQ(headers_get(h, "CoNtEnT-tYpE"), "text/html");
    destroy_headers(h);
}

TEST(original_spelling_is_preserved)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_set(h, "X-Custom-Header", "v"), 0);

    collect_ctx_t ctx = {0};
    CHECK_INT_EQ(headers_foreach(h, collect, &ctx), 0);
    CHECK_INT_EQ(ctx.count, 1);
    CHECK_TRUE(ctx_has_name(&ctx, "X-Custom-Header"));
    destroy_headers(h);
}

TEST(set_replaces_existing_values)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_add(h, "Accept", "text/html"), 0);
    CHECK_INT_EQ(headers_add(h, "Accept", "application/json"), 0);
    CHECK_INT_EQ(headers_count(h), 2);

    CHECK_INT_EQ(headers_set(h, "Accept", "text/plain"), 0);
    CHECK_INT_EQ(headers_count(h), 1);
    CHECK_STR_EQ(headers_get(h, "Accept"), "text/plain");
    destroy_headers(h);
}

TEST(set_does_not_touch_other_names)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_set(h, "A", "1"), 0);
    CHECK_INT_EQ(headers_set(h, "B", "2"), 0);
    CHECK_INT_EQ(headers_set(h, "A", "3"), 0);
    CHECK_INT_EQ(headers_count(h), 2);
    CHECK_STR_EQ(headers_get(h, "A"), "3");
    CHECK_STR_EQ(headers_get(h, "B"), "2");
    destroy_headers(h);
}

TEST(set_collapses_duplicate_case_variants)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_add(h, "X-A", "1"), 0);
    CHECK_INT_EQ(headers_add(h, "x-a", "2"), 0);
    CHECK_INT_EQ(headers_count(h), 2);

    CHECK_INT_EQ(headers_set(h, "X-A", "3"), 0);
    CHECK_INT_EQ(headers_count(h), 1);
    CHECK_STR_EQ(headers_get(h, "x-a"), "3");
    destroy_headers(h);
}

/* ------------------------------------------------------------------------- */
/* add                                                                       */
/* ------------------------------------------------------------------------- */

TEST(add_appends_without_replacing)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_add(h, "Set-Cookie", "a=1"), 0);
    CHECK_INT_EQ(headers_add(h, "Set-Cookie", "b=2"), 0);
    CHECK_INT_EQ(headers_count(h), 2);

    collect_ctx_t ctx = {0};
    CHECK_INT_EQ(headers_foreach(h, collect, &ctx), 0);
    CHECK_INT_EQ(ctx.count, 2);
    CHECK_TRUE(ctx_has_value(&ctx, "a=1"));
    CHECK_TRUE(ctx_has_value(&ctx, "b=2"));
    CHECK_NOT_NULL(headers_get(h, "Set-Cookie"));
    destroy_headers(h);
}

/* ------------------------------------------------------------------------- */
/* remove / contains                                                         */
/* ------------------------------------------------------------------------- */

TEST(remove_deletes_all_values)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_add(h, "A", "1"), 0);
    CHECK_INT_EQ(headers_add(h, "A", "2"), 0);
    CHECK_INT_EQ(headers_set(h, "B", "3"), 0);
    CHECK_INT_EQ(headers_count(h), 3);

    CHECK_INT_EQ(headers_remove(h, "a"), 0);
    CHECK_INT_EQ(headers_count(h), 1);
    CHECK_FALSE(headers_contains(h, "A"));
    CHECK_STR_EQ(headers_get(h, "B"), "3");
    destroy_headers(h);
}

TEST(remove_absent_returns_minus_one)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_remove(h, "Missing"), -1);
    destroy_headers(h);
}

TEST(contains_is_case_insensitive)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    CHECK_INT_EQ(headers_set(h, "X-Test", "1"), 0);
    CHECK_TRUE(headers_contains(h, "x-test"));
    CHECK_TRUE(headers_contains(h, "X-TEST"));
    CHECK_FALSE(headers_contains(h, "X-Other"));
    CHECK_FALSE(headers_contains(NULL, "X-Test"));
    destroy_headers(h);
}

TEST(count_null_is_zero)
{
    CHECK_INT_EQ(headers_count(NULL), 0);
}

/* ------------------------------------------------------------------------- */
/* foreach                                                                   */
/* ------------------------------------------------------------------------- */

TEST(foreach_visits_every_entry)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    for (int i = 0; i < 5; i++)
    {
        char name[16];
        snprintf(name, sizeof(name), "H%d", i);
        CHECK_INT_EQ(headers_set(h, name, "x"), 0);
    }

    collect_ctx_t ctx = {0};
    CHECK_INT_EQ(headers_foreach(h, collect, &ctx), 0);
    CHECK_INT_EQ(ctx.count, 5);
    destroy_headers(h);
}

TEST(foreach_stops_early_and_returns_callback_value)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    for (int i = 0; i < 5; i++)
    {
        char name[16];
        snprintf(name, sizeof(name), "H%d", i);
        CHECK_INT_EQ(headers_set(h, name, "x"), 0);
    }

    collect_ctx_t ctx = {.stop_after = 2};
    CHECK_INT_EQ(headers_foreach(h, collect, &ctx), 42);
    CHECK_INT_EQ(ctx.count, 2);
    destroy_headers(h);
}

TEST(foreach_invalid_arguments)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);
    collect_ctx_t ctx = {0};
    CHECK_INT_EQ(headers_foreach(NULL, collect, &ctx), -1);
    CHECK_INT_EQ(headers_foreach(h, NULL, &ctx), -1);
    destroy_headers(h);
}

/* ------------------------------------------------------------------------- */
/* Invalid arguments                                                         */
/* ------------------------------------------------------------------------- */

TEST(invalid_arguments_are_rejected)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);

    CHECK_INT_EQ(headers_add(NULL, "A", "1"), -1);
    CHECK_INT_EQ(headers_add(h, NULL, "1"), -1);
    CHECK_INT_EQ(headers_add(h, "A", NULL), -1);

    CHECK_INT_EQ(headers_set(NULL, "A", "1"), -1);
    CHECK_INT_EQ(headers_set(h, NULL, "1"), -1);
    CHECK_INT_EQ(headers_set(h, "A", NULL), -1);

    CHECK_NULL(headers_get(NULL, "A"));
    CHECK_NULL(headers_get(h, NULL));
    CHECK_NULL(headers_get(h, "Absent"));

    CHECK_INT_EQ(headers_remove(NULL, "A"), -1);
    CHECK_INT_EQ(headers_remove(h, NULL), -1);

    CHECK_INT_EQ(headers_count(h), 0);
    destroy_headers(h);
}

/* ------------------------------------------------------------------------- */
/* Growth / rehash                                                           */
/* ------------------------------------------------------------------------- */

TEST(rehash_preserves_entries)
{
    headers_t *h = create_headers();
    REQUIRE(h != NULL);

    const int n = 200; /* well past HEADERS_DEFAULT_CAPACITY * 3 / 4 */
    char name[32];
    char value[32];
    for (int i = 0; i < n; i++)
    {
        snprintf(name, sizeof(name), "Header-%d", i);
        snprintf(value, sizeof(value), "value-%d", i);
        REQUIRE(headers_set(h, name, value) == 0);
    }
    CHECK_INT_EQ(headers_count(h), (size_t)n);

    for (int i = 0; i < n; i++)
    {
        snprintf(name, sizeof(name), "Header-%d", i);
        snprintf(value, sizeof(value), "value-%d", i);
        CHECK_STR_EQ(headers_get(h, name), value);
    }

    destroy_headers(h);
}

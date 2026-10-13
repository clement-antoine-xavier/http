#pragma once

#include <stddef.h>
#include <stdbool.h>

/**
 * @file headers.h
 * @brief Case-insensitive HTTP header map backed by a chained hash table.
 *
 * Header names are compared case-insensitively as required by RFC 9110 while
 * their original spelling is preserved. A single name may hold several values
 * so that repeated header fields can be represented faithfully.
 */

/**
 * @brief Number of buckets allocated by create_headers().
 */
#define HEADERS_DEFAULT_CAPACITY 16u

/**
 * @brief A single stored header name/value pair.
 *
 * Entries form a singly linked list. The same link is used both to resolve
 * hash collisions between different names and to chain repeated occurrences
 * of one name.
 */
typedef struct header_entry_s
{
    char *name;                  /**< Owned, NUL-terminated name (original case). */
    char *value;                 /**< Owned, NUL-terminated value. */
    struct header_entry_s *next; /**< Next entry in the bucket chain, or NULL. */
} header_entry_t;

/**
 * @brief Case-insensitive map of HTTP header names to values.
 *
 * @note Treat instances as opaque; use the headers_* functions to modify them.
 */
typedef struct headers_s
{
    header_entry_t **buckets; /**< Bucket array of chained entries. */
    size_t capacity;          /**< Current number of buckets. */
    size_t size;              /**< Total stored name/value pairs. */
} headers_t;

/**
 * @brief Visitor callback invoked by headers_foreach() for every entry.
 *
 * @param[in] name  Header name; never NULL.
 * @param[in] value Header value; never NULL.
 * @param[in] ctx   Opaque context supplied to headers_foreach().
 * @return 0 to keep iterating, nonzero to stop early.
 */
typedef int (*headers_iter_fn)(const char *name, const char *value, void *ctx);

/**
 * @brief Allocate an empty header map.
 *
 * @return A new map with HEADERS_DEFAULT_CAPACITY buckets, or NULL on
 *         allocation failure.
 */
headers_t *create_headers(void);

/**
 * @brief Release a map and every entry it owns.
 *
 * @param[in] headers Map to destroy; may be NULL.
 */
void destroy_headers(headers_t *headers);

/**
 * @brief Insert or replace the values stored under a name.
 *
 * Any values already stored for @p name are discarded and replaced by a single
 * copy of @p value.
 *
 * @param[in,out] headers Map to modify.
 * @param[in]     name    Header name, matched case-insensitively.
 * @param[in]     value   Value to store; copied internally.
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int headers_set(headers_t *headers, const char *name, const char *value);

/**
 * @brief Append another value under a name without removing existing ones.
 *
 * Use this to represent repeated header fields.
 *
 * @param[in,out] headers Map to modify.
 * @param[in]     name    Header name, matched case-insensitively.
 * @param[in]     value   Value to append; copied internally.
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int headers_add(headers_t *headers, const char *name, const char *value);

/**
 * @brief Look up the first value stored under a name.
 *
 * @param[in] headers Map to search.
 * @param[in] name    Header name, matched case-insensitively.
 * @return A borrowed pointer to the stored value, or NULL if absent. The
 *         pointer is invalidated by any later mutation or destroy_headers().
 */
const char *headers_get(const headers_t *headers, const char *name);

/**
 * @brief Remove a name and all of its values.
 *
 * @param[in,out] headers Map to modify.
 * @param[in]     name    Header name, matched case-insensitively.
 * @return 0 if at least one value was removed, -1 otherwise.
 */
int headers_remove(headers_t *headers, const char *name);

/**
 * @brief Test whether a name is present.
 *
 * @param[in] headers Map to search.
 * @param[in] name    Header name, matched case-insensitively.
 * @return true if the name has at least one value, false otherwise.
 */
bool headers_contains(const headers_t *headers, const char *name);

/**
 * @brief Return the total number of stored name/value pairs.
 *
 * @param[in] headers Map to inspect; may be NULL.
 * @return The entry count, or 0 if @p headers is NULL.
 */
size_t headers_count(const headers_t *headers);

/**
 * @brief Visit every stored name/value pair.
 *
 * Iteration order is unspecified.
 *
 * @param[in] headers Map to iterate.
 * @param[in] fn      Callback invoked once per entry; must not be NULL.
 * @param[in] ctx     Opaque context forwarded to @p fn.
 * @return 0 if all entries were visited or the callback stopped with 0, the
 *         nonzero value returned by the callback if it stopped early, or -1 on
 *         invalid arguments.
 */
int headers_foreach(const headers_t *headers, headers_iter_fn fn, void *ctx);

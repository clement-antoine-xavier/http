#include "http/headers.h"

#include <stdlib.h>
#include <string.h>

static size_t hash_ci(const char *name)
{
    size_t hash = 14695981039346656037ull;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
    {
        unsigned char c = *p;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c + ('a' - 'A'));
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int name_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (unsigned char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z')
            cb = (unsigned char)(cb + ('a' - 'A'));
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy)
        memcpy(copy, s, n);
    return copy;
}

static header_entry_t *find(const headers_t *headers, const char *name)
{
    size_t index = hash_ci(name) % headers->capacity;
    for (header_entry_t *entry = headers->buckets[index]; entry; entry = entry->next)
    {
        if (name_eq(entry->name, name))
            return entry;
    }
    return NULL;
}

static int rehash(headers_t *headers, size_t capacity)
{
    header_entry_t **buckets = calloc(capacity, sizeof(*buckets));
    if (!buckets)
        return -1;
    for (size_t i = 0; i < headers->capacity; i++)
    {
        header_entry_t *entry = headers->buckets[i];
        while (entry)
        {
            header_entry_t *next = entry->next;
            size_t index = hash_ci(entry->name) % capacity;
            entry->next = buckets[index];
            buckets[index] = entry;
            entry = next;
        }
    }
    free(headers->buckets);
    headers->buckets = buckets;
    headers->capacity = capacity;
    return 0;
}

headers_t *create_headers(void)
{
    headers_t *headers = malloc(sizeof(*headers));
    if (!headers)
        return NULL;
    headers->buckets = calloc(HEADERS_DEFAULT_CAPACITY, sizeof(*headers->buckets));
    if (!headers->buckets)
    {
        free(headers);
        return NULL;
    }
    headers->capacity = HEADERS_DEFAULT_CAPACITY;
    headers->size = 0;
    return headers;
}

void destroy_headers(headers_t *headers)
{
    if (!headers)
        return;
    for (size_t i = 0; i < headers->capacity; i++)
    {
        header_entry_t *entry = headers->buckets[i];
        while (entry)
        {
            header_entry_t *next = entry->next;
            free(entry->name);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(headers->buckets);
    free(headers);
}

int headers_add(headers_t *headers, const char *name, const char *value)
{
    if (!headers || !name || !value)
        return -1;
    if (headers->size + 1 > headers->capacity * 3 / 4)
    {
        if (rehash(headers, headers->capacity * 2) != 0)
            return -1;
    }
    header_entry_t *entry = malloc(sizeof(*entry));
    if (!entry)
        return -1;
    entry->name = dup_str(name);
    entry->value = dup_str(value);
    if (!entry->name || !entry->value)
    {
        free(entry->name);
        free(entry->value);
        free(entry);
        return -1;
    }
    size_t index = hash_ci(name) % headers->capacity;
    entry->next = headers->buckets[index];
    headers->buckets[index] = entry;
    headers->size++;
    return 0;
}

int headers_set(headers_t *headers, const char *name, const char *value)
{
    if (!headers || !name || !value)
        return -1;
    header_entry_t *entry = find(headers, name);
    if (!entry)
        return headers_add(headers, name, value);
    char *copy = dup_str(value);
    if (!copy)
        return -1;
    free(entry->value);
    entry->value = copy;
    size_t index = hash_ci(name) % headers->capacity;
    header_entry_t **link = &headers->buckets[index];
    while (*link)
    {
        header_entry_t *current = *link;
        if (current != entry && name_eq(current->name, name))
        {
            *link = current->next;
            free(current->name);
            free(current->value);
            free(current);
            headers->size--;
        }
        else
        {
            link = &current->next;
        }
    }
    return 0;
}

const char *headers_get(const headers_t *headers, const char *name)
{
    if (!headers || !name)
        return NULL;
    header_entry_t *entry = find(headers, name);
    return entry ? entry->value : NULL;
}

int headers_remove(headers_t *headers, const char *name)
{
    if (!headers || !name)
        return -1;
    size_t index = hash_ci(name) % headers->capacity;
    header_entry_t **link = &headers->buckets[index];
    int removed = 0;
    while (*link)
    {
        header_entry_t *entry = *link;
        if (name_eq(entry->name, name))
        {
            *link = entry->next;
            free(entry->name);
            free(entry->value);
            free(entry);
            headers->size--;
            removed++;
        }
        else
        {
            link = &entry->next;
        }
    }
    return removed > 0 ? 0 : -1;
}

bool headers_contains(const headers_t *headers, const char *name)
{
    if (!headers || !name)
        return false;
    return find(headers, name) != NULL;
}

size_t headers_count(const headers_t *headers)
{
    return headers ? headers->size : 0;
}

int headers_foreach(const headers_t *headers, headers_iter_fn fn, void *ctx)
{
    if (!headers || !fn)
        return -1;
    for (size_t i = 0; i < headers->capacity; i++)
    {
        for (header_entry_t *entry = headers->buckets[i]; entry; entry = entry->next)
        {
            int result = fn(entry->name, entry->value, ctx);
            if (result)
                return result;
        }
    }
    return 0;
}

#pragma once

#include "http/headers.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file message_internal.h
 * @brief Private helpers shared by the request and response parsers.
 *
 * The wire format handling is identical for requests and responses once the
 * start line has been consumed, so the line splitting, header-block
 * (de)serialization, body framing and chunked transfer coding live here. This
 * header is only ever included by src/request.c and src/response.c and defines
 * static inline helpers, so it needs no build wiring of its own.
 */

/* ------------------------------------------------------------------------- */
/* Strings                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Binary-safe copy of @p n bytes starting at @p s, NUL-terminated.
 *
 * @return A newly allocated copy, or NULL on allocation failure.
 */
static inline char *msg_dup_n(const char *s, size_t n)
{
    char *copy = malloc(n + 1);
    if (!copy)
        return NULL;
    if (n)
        memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

/**
 * @brief Binary-safe copy of a NUL-terminated string.
 */
static inline char *msg_strdup(const char *s)
{
    return msg_dup_n(s, strlen(s));
}

/**
 * @brief Case-insensitive equality of two NUL-terminated strings.
 */
static inline int msg_ci_equal(const char *a, const char *b)
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
    return *a == '\0' && *b == '\0';
}

/* ------------------------------------------------------------------------- */
/* Growable byte buffer                                                      */
/* ------------------------------------------------------------------------- */

/** @brief Append-only byte buffer used while (de)serializing messages. */
typedef struct msg_buf_s
{
    char *data; /**< Owned buffer, NUL-terminated for convenience. */
    size_t len; /**< Number of meaningful bytes. */
    size_t cap; /**< Allocated capacity, excluding the NUL terminator. */
} msg_buf_t;

/** @brief Initialise an empty buffer. */
static inline void msg_buf_init(msg_buf_t *buf)
{
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

/** @brief Release a buffer's storage; the handle must not be used afterwards. */
static inline void msg_buf_free(msg_buf_t *buf)
{
    free(buf->data);
    msg_buf_init(buf);
}

/**
 * @brief Ensure room for @p extra more bytes.
 *
 * @return 0 on success, -1 on allocation failure or size overflow.
 */
static inline int msg_buf_reserve(msg_buf_t *buf, size_t extra)
{
    if (extra > SIZE_MAX - buf->len)
        return -1;
    size_t needed = buf->len + extra;
    if (buf->data && needed <= buf->cap)
        return 0;
    size_t cap = buf->cap ? buf->cap : 64;
    while (cap < needed)
    {
        if (cap > SIZE_MAX / 2)
        {
            cap = needed;
            break;
        }
        cap *= 2;
    }
    char *data = realloc(buf->data, cap + 1);
    if (!data)
        return -1;
    buf->data = data;
    buf->cap = cap;
    return 0;
}

/**
 * @brief Append @p n bytes from @p s.
 *
 * @note @p s must not point into the buffer being appended to.
 * @return 0 on success, -1 on allocation failure.
 */
static inline int msg_buf_append(msg_buf_t *buf, const char *s, size_t n)
{
    if (msg_buf_reserve(buf, n) != 0)
        return -1;
    if (n)
        memcpy(buf->data + buf->len, s, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
    return 0;
}

/** @brief Append a NUL-terminated string. */
static inline int msg_buf_puts(msg_buf_t *buf, const char *s)
{
    return msg_buf_append(buf, s, strlen(s));
}

/** @brief Append a single byte. */
static inline int msg_buf_putc(msg_buf_t *buf, char c)
{
    return msg_buf_append(buf, &c, 1);
}

/* ------------------------------------------------------------------------- */
/* Lexical helpers                                                           */
/* ------------------------------------------------------------------------- */

/** @brief Test whether @p c is an RFC 9110 tchar. */
static inline int msg_is_tchar(unsigned char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z'))
        return 1;
    switch (c)
    {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
        return 1;
    default:
        return 0;
    }
}

/** @brief Test whether [@p s, @p s + @p len) is a non-empty token. */
static inline int msg_is_token(const char *s, size_t len)
{
    if (!s || len == 0)
        return 0;
    for (size_t i = 0; i < len; i++)
        if (!msg_is_tchar((unsigned char)s[i]))
            return 0;
    return 1;
}

/** @brief Test whether [@p s, @p s + @p len) matches "HTTP/" 1*DIGIT "." 1*DIGIT. */
static inline int msg_is_http_version_n(const char *s, size_t len)
{
    if (!s || len < 8 || memcmp(s, "HTTP/", 5) != 0)
        return 0;
    size_t i = 5;
    size_t digits = 0;
    while (i < len && s[i] >= '0' && s[i] <= '9')
    {
        i++;
        digits++;
    }
    if (digits == 0 || i >= len || s[i] != '.')
        return 0;
    i++;
    digits = 0;
    while (i < len && s[i] >= '0' && s[i] <= '9')
    {
        i++;
        digits++;
    }
    return digits > 0 && i == len;
}

/** @brief NUL-terminated variant of msg_is_http_version_n(). */
static inline int msg_is_http_version(const char *s)
{
    return s && msg_is_http_version_n(s, strlen(s));
}

/** @brief Test for optional whitespace (SP / HTAB). */
static inline int msg_is_ows(unsigned char c)
{
    return c == ' ' || c == '\t';
}

/**
 * @brief Find the first CRLF in [@p start, @p end).
 *
 * @return A pointer to the '\r', or NULL if the range holds no CRLF.
 */
static inline const char *msg_find_crlf(const char *start, const char *end)
{
    for (const char *p = start; p + 1 < end; p++)
        if (p[0] == '\r' && p[1] == '\n')
            return p;
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Header block                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Parse one "name: value" field line into @p headers.
 *
 * Leading/trailing optional whitespace around the value is trimmed and the
 * name is validated as a token. Obs-fold (a continuation line starting with
 * whitespace) is rejected because its name would not be a token.
 *
 * @return 0 on success, -1 on a malformed line or allocation failure.
 */
static inline int msg_parse_header_line(headers_t *headers, const char *line,
                                        size_t len)
{
    const char *colon = memchr(line, ':', len);
    if (!colon)
        return -1;
    size_t name_len = (size_t)(colon - line);
    if (!msg_is_token(line, name_len))
        return -1;

    const char *value = colon + 1;
    const char *value_end = line + len;
    while (value < value_end && msg_is_ows((unsigned char)*value))
        value++;
    while (value_end > value && msg_is_ows((unsigned char)value_end[-1]))
        value_end--;

    char *name = msg_dup_n(line, name_len);
    char *val = msg_dup_n(value, (size_t)(value_end - value));
    int rc = 0;
    if (!name || !val)
        rc = -1;
    else
        rc = headers_add(headers, name, val);
    free(name);
    free(val);
    return rc;
}

/**
 * @brief Parse a header block up to and including its terminating blank line.
 *
 * On success @p *cursor is advanced past the blank line that ends the block.
 *
 * @return 0 on success, -1 on a malformed block or allocation failure.
 */
static inline int msg_parse_headers(headers_t *headers, const char **cursor,
                                    const char *end)
{
    const char *p = *cursor;
    for (;;)
    {
        const char *crlf = msg_find_crlf(p, end);
        if (!crlf)
            return -1;
        if (crlf == p)
        {
            *cursor = crlf + 2;
            return 0;
        }
        if (msg_parse_header_line(headers, p, (size_t)(crlf - p)) != 0)
            return -1;
        p = crlf + 2;
    }
}

/** @brief Context for msg_serialize_header_cb(). */
typedef struct msg_header_ctx_s
{
    msg_buf_t *buf;
    int error;
} msg_header_ctx_t;

/** @brief headers_iter_fn that appends "name: value\r\n" to a buffer. */
static inline int msg_serialize_header_cb(const char *name, const char *value,
                                          void *ctx)
{
    msg_header_ctx_t *c = ctx;
    if (msg_buf_puts(c->buf, name) != 0 || msg_buf_puts(c->buf, ": ") != 0 ||
        msg_buf_puts(c->buf, value) != 0 || msg_buf_puts(c->buf, "\r\n") != 0)
    {
        c->error = 1;
        return 1;
    }
    return 0;
}

/**
 * @brief Serialize every field in @p headers as "name: value\r\n".
 *
 * @return 0 on success, -1 on allocation failure. A NULL @p headers is treated
 *         as empty.
 */
static inline int msg_serialize_headers(const headers_t *headers, msg_buf_t *buf)
{
    if (!headers)
        return 0;
    msg_header_ctx_t ctx = {.buf = buf, .error = 0};
    int rc = headers_foreach(headers, msg_serialize_header_cb, &ctx);
    if (ctx.error || rc != 0)
        return -1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Header lookups                                                            */
/* ------------------------------------------------------------------------- */

/** @brief Test whether @p value contains @p token as a comma-separated item. */
static inline int msg_value_has_token(const char *value, const char *token)
{
    size_t token_len = strlen(token);
    const char *p = value;
    while (*p)
    {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        const char *start = p;
        while (*p && *p != ',')
            p++;
        const char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        if ((size_t)(end - start) == token_len)
        {
            size_t i = 0;
            for (; i < token_len; i++)
            {
                char a = start[i];
                char b = token[i];
                if (a >= 'A' && a <= 'Z')
                    a = (char)(a + ('a' - 'A'));
                if (b >= 'A' && b <= 'Z')
                    b = (char)(b + ('a' - 'A'));
                if (a != b)
                    break;
            }
            if (i == token_len)
                return 1;
        }
        if (*p == ',')
            p++;
    }
    return 0;
}

/** @brief Context for msg_find_token_cb(). */
typedef struct msg_find_ctx_s
{
    const char *name;
    const char *token;
    int found;
} msg_find_ctx_t;

static inline int msg_find_token_cb(const char *name, const char *value, void *ctx)
{
    msg_find_ctx_t *c = ctx;
    if (msg_ci_equal(name, c->name) && msg_value_has_token(value, c->token))
    {
        c->found = 1;
        return 1;
    }
    return 0;
}

/**
 * @brief Test whether any occurrence of @p name contains @p token.
 *
 * @return 1 if found, 0 otherwise (including a NULL map).
 */
static inline int msg_has_token(const headers_t *headers, const char *name,
                                const char *token)
{
    if (!headers)
        return 0;
    msg_find_ctx_t ctx = {.name = name, .token = token, .found = 0};
    headers_foreach(headers, msg_find_token_cb, &ctx);
    return ctx.found;
}

/** @brief Context for msg_content_length_cb(). */
typedef struct msg_length_ctx_s
{
    size_t value;
    int seen;
    int error;
} msg_length_ctx_t;

static inline int msg_content_length_cb(const char *name, const char *value,
                                        void *ctx)
{
    msg_length_ctx_t *c = ctx;
    if (!msg_ci_equal(name, "Content-Length"))
        return 0;
    if (*value == '\0')
    {
        c->error = 1;
        return 1;
    }
    size_t result = 0;
    for (const char *p = value; *p; p++)
    {
        if (*p < '0' || *p > '9' ||
            result > (SIZE_MAX - (size_t)(*p - '0')) / 10)
        {
            c->error = 1;
            return 1;
        }
        result = result * 10 + (size_t)(*p - '0');
    }
    if (c->seen && c->value != result) /* conflicting occurrences */
    {
        c->error = 1;
        return 1;
    }
    c->value = result;
    c->seen = 1;
    return 0;
}

/**
 * @brief Look up and validate Content-Length.
 *
 * @param[out] out     Receives the parsed length.
 * @param[out] present Receives 1 if the header was present, 0 otherwise.
 * @return 0 on success, -1 if the value is malformed or occurrences disagree.
 */
static inline int msg_content_length(const headers_t *headers, size_t *out,
                                     int *present)
{
    msg_length_ctx_t ctx = {.value = 0, .seen = 0, .error = 0};
    if (headers)
        headers_foreach(headers, msg_content_length_cb, &ctx);
    if (ctx.error)
        return -1;
    *out = ctx.value;
    *present = ctx.seen;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Chunked transfer coding                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Parse a hexadecimal chunk size.
 *
 * @return 0 on success, -1 on a non-hex or overflowing value.
 */
static inline int msg_hex_value(const char *s, size_t len, size_t *out)
{
    if (len == 0)
        return -1;
    size_t value = 0;
    for (size_t i = 0; i < len; i++)
    {
        char c = s[i];
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return -1;
        if (value > (SIZE_MAX - (size_t)digit) / 16)
            return -1;
        value = value * 16 + (size_t)digit;
    }
    *out = value;
    return 0;
}

/**
 * @brief Decode a chunked body starting at @p *cursor.
 *
 * Chunk extensions are ignored and the trailer section is consumed and
 * discarded. On success @p *cursor is left just past the trailer section's
 * terminating blank line.
 *
 * @return 0 on success, -1 on malformed framing or allocation failure.
 */
static inline int msg_decode_chunked(const char **cursor, const char *end,
                                     msg_buf_t *out)
{
    const char *p = *cursor;
    for (;;)
    {
        const char *crlf = msg_find_crlf(p, end);
        if (!crlf)
            return -1;
        const char *size_end = crlf;
        for (const char *q = p; q < crlf; q++)
        {
            if (*q == ';') /* strip any chunk extensions */
            {
                size_end = q;
                break;
            }
        }
        size_t size = 0;
        if (msg_hex_value(p, (size_t)(size_end - p), &size) != 0)
            return -1;
        p = crlf + 2;
        if (size == 0)
            break;
        size_t remaining = (size_t)(end - p);
        if (size > remaining || remaining - size < 2)
            return -1;
        if (msg_buf_append(out, p, size) != 0)
            return -1;
        p += size;
        if (p[0] != '\r' || p[1] != '\n')
            return -1;
        p += 2;
    }

    /* Consume the trailer section: field lines up to a blank line. */
    for (;;)
    {
        const char *line = p;
        const char *crlf = msg_find_crlf(p, end);
        if (!crlf)
            return -1;
        p = crlf + 2;
        if (crlf == line) /* empty line: end of trailers */
            break;
    }
    *cursor = p;
    return 0;
}

/**
 * @brief Append @p len bytes as a single chunk followed by the terminator.
 *
 * An empty body still emits the "0\r\n\r\n" terminator.
 */
static inline int msg_serialize_chunked(const char *body, size_t len,
                                        msg_buf_t *buf)
{
    if (len > 0)
    {
        char size[2 * sizeof(size_t) + 1];
        char *w = size + sizeof(size);
        *--w = '\0';
        size_t n = len;
        do
        {
            *--w = "0123456789abcdef"[n % 16];
            n /= 16;
        } while (n);
        if (msg_buf_puts(buf, w) != 0 || msg_buf_puts(buf, "\r\n") != 0 ||
            msg_buf_append(buf, body, len) != 0 || msg_buf_puts(buf, "\r\n") != 0)
            return -1;
    }
    return msg_buf_puts(buf, "0\r\n\r\n");
}

/* ------------------------------------------------------------------------- */
/* Body framing                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Read a message body starting at @p *cursor according to the framing
 *        headers.
 *
 * Chunked transfer coding takes precedence over Content-Length; otherwise an
 * explicit Content-Length is honoured and any leftover bytes are the body. The
 * decoded bytes are binary-safe and @p *body is NULL when the body is empty.
 *
 * @return 0 on success, -1 on malformed framing or allocation failure.
 */
static inline int msg_parse_body(const headers_t *headers, const char **cursor,
                                 const char *end, char **body, size_t *body_len)
{
    const char *p = *cursor;
    msg_buf_t buf;
    msg_buf_init(&buf);
    int rc;

    if (msg_has_token(headers, "Transfer-Encoding", "chunked"))
    {
        rc = msg_decode_chunked(&p, end, &buf);
    }
    else
    {
        int present = 0;
        size_t length = 0;
        rc = msg_content_length(headers, &length, &present);
        if (rc == 0)
        {
            size_t remaining = (size_t)(end - p);
            if (present && length > remaining)
            {
                rc = -1;
            }
            else
            {
                size_t take = present ? length : remaining;
                if (msg_buf_append(&buf, p, take) != 0)
                    rc = -1;
                else
                    p += take;
            }
        }
    }

    if (rc != 0)
    {
        msg_buf_free(&buf);
        return -1;
    }
    if (buf.len == 0)
    {
        msg_buf_free(&buf);
        *body = NULL;
    }
    else
    {
        *body = buf.data;
    }
    *body_len = buf.len;
    *cursor = p;
    return 0;
}

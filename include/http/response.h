#pragma once

#include "http/headers.h"

#include <stddef.h>

/**
 * @file response.h
 * @brief HTTP response message model plus wire-format parsing and serialization.
 *
 * A response_t couples an HTTP version, a status code, an optional reason
 * phrase, a header map and an optional body. Bodies are stored as raw bytes
 * together with their length so binary payloads survive a round-trip.
 *
 * response_parse() reads a complete message (status line, headers and body)
 * from a buffer; response_to_string() renders one back to the wire. Framing
 * rules follow RFC 9112: chunked transfer coding takes precedence over
 * Content-Length, and the body is otherwise the remaining bytes.
 *
 * @code
 * response_t *res = response_parse(wire, wire_len);
 * if (res)
 * {
 *     printf("%d %s\n", response_get_status(res),
 *            response_get_reason(res) ? response_get_reason(res) : "");
 *     destroy_response(res);
 * }
 * @endcode
 */

/**
 * @brief A parsed or programmatically built HTTP response.
 *
 * @note Treat instances as opaque; create them with create_response() and
 *       release them with destroy_response(). All members are owned by the
 *       response_t.
 */
typedef struct response_s
{
    char *version;      /**< Owned HTTP version, e.g. "HTTP/1.1". */
    int status;         /**< Status code, e.g. 200. */
    char *reason;       /**< Owned reason phrase; NULL when absent. */
    headers_t *headers; /**< Owned header map. */
    char *body;         /**< Owned body bytes; NULL when empty. */
    size_t body_len;    /**< Number of body bytes. */
} response_t;

/**
 * @brief Allocate a response with sensible defaults.
 *
 * The new response is "HTTP/1.1 200" with no reason phrase, an empty header map
 * and no body.
 *
 * @return A new response, or NULL on allocation failure. Release it with
 *         destroy_response().
 */
response_t *create_response(void);

/**
 * @brief Release a response and everything it owns.
 *
 * @param[in] response Response to destroy; may be NULL.
 */
void destroy_response(response_t *response);

/**
 * @brief Return the HTTP version.
 *
 * @param[in] response Response to inspect; may be NULL.
 * @return A borrowed version string, or NULL if @p response is NULL.
 */
const char *response_get_version(const response_t *response);

/**
 * @brief Return the status code.
 *
 * @param[in] response Response to inspect; may be NULL.
 * @return The status code, or 0 if @p response is NULL.
 */
int response_get_status(const response_t *response);

/**
 * @brief Return the reason phrase.
 *
 * @param[in] response Response to inspect; may be NULL.
 * @return A borrowed reason phrase, or NULL if absent or @p response is NULL.
 */
const char *response_get_reason(const response_t *response);

/**
 * @brief Return the mutable header map.
 *
 * Use the headers_* functions to add, replace or remove fields.
 *
 * @param[in] response Response to inspect; may be NULL.
 * @return A borrowed header map, or NULL if @p response is NULL.
 */
headers_t *response_get_headers(response_t *response);

/**
 * @brief Return the body bytes.
 *
 * @param[in]  response Response to inspect; may be NULL.
 * @param[out] len      Receives the body length; may be NULL.
 * @return A borrowed body pointer (NULL when empty), or NULL if @p response is
 *         NULL.
 */
const char *response_get_body(const response_t *response, size_t *len);

/**
 * @brief Set the HTTP version.
 *
 * @param[in,out] response Response to modify.
 * @param[in]     version  Version of the form "HTTP/<major>.<minor>".
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int response_set_version(response_t *response, const char *version);

/**
 * @brief Set the status code.
 *
 * @param[in,out] response Response to modify.
 * @param[in]     status   Code in the range 100..999.
 * @return 0 on success, -1 on an out-of-range code.
 */
int response_set_status(response_t *response, int status);

/**
 * @brief Set or clear the reason phrase.
 *
 * @param[in,out] response Response to modify.
 * @param[in]     reason   New phrase, or NULL to clear it. Must not contain CR
 *                         or LF.
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int response_set_reason(response_t *response, const char *reason);

/**
 * @brief Replace the body with a copy of @p body.
 *
 * @param[in,out] response Response to modify.
 * @param[in]     body     Bytes to copy; may be NULL only when @p len is 0.
 * @param[in]     len      Number of bytes.
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int response_set_body(response_t *response, const char *body, size_t len);

/**
 * @brief Parse a complete response from @p data.
 *
 * @param[in] data Wire bytes. May contain NUL bytes in the body.
 * @param[in] len  Number of bytes in @p data.
 * @return A new response, or NULL if @p data is NULL or malformed. Release it
 *         with destroy_response().
 */
response_t *response_parse(const char *data, size_t len);

/**
 * @brief Serialize a response to the wire format.
 *
 * The status line, headers and body are rendered in order. Framing headers are
 * never added or corrected; if Transfer-Encoding contains "chunked" the body is
 * re-encoded as a single chunk.
 *
 * @param[in]  response Response to serialize.
 * @param[out] len      Receives the byte length; may be NULL. The result is also
 *                      NUL-terminated, but may contain embedded NULs.
 * @return A newly allocated string the caller must free(), or NULL on invalid
 *         arguments or allocation failure.
 */
char *response_to_string(const response_t *response, size_t *len);

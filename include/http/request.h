#pragma once

#include "http/headers.h"
#include "http/url.h"

#include <stddef.h>

/**
 * @file request.h
 * @brief HTTP request message model plus wire-format parsing and serialization.
 *
 * A request_t couples a method, a request-target, an HTTP version, a header map
 * and an optional body. Bodies are stored as raw bytes together with their
 * length so binary payloads survive a round-trip.
 *
 * request_parse() reads a complete message (headers plus body) from a buffer;
 * request_to_string() renders one back to the wire. Framing rules follow RFC
 * 9112: chunked transfer coding takes precedence over Content-Length, and the
 * body is otherwise the remaining bytes.
 *
 * @code
 * request_t *req = create_request();
 * request_set_method(req, "GET");
 * request_set_url(req, "/index.html");
 * headers_set(request_get_headers(req), "Host", "example.com");
 * char *wire = request_to_string(req, NULL);
 * destroy_request(req);
 * @endcode
 */

/**
 * @brief A parsed or programmatically built HTTP request.
 *
 * @note Treat instances as opaque; create them with create_request() and
 *       release them with destroy_request(). All members are owned by the
 *       request_t.
 */
typedef struct request_s
{
    char *method;       /**< Owned method token, e.g. "GET". */
    url_t *url;         /**< Owned, parsed request-target. */
    char *version;      /**< Owned HTTP version, e.g. "HTTP/1.1". */
    headers_t *headers; /**< Owned header map. */
    char *body;         /**< Owned body bytes; NULL when empty. */
    size_t body_len;    /**< Number of body bytes. */
} request_t;

/**
 * @brief Allocate a request with sensible defaults.
 *
 * The new request is "GET / HTTP/1.1" with an empty header map and no body.
 *
 * @return A new request, or NULL on allocation failure. Release it with
 *         destroy_request().
 */
request_t *create_request(void);

/**
 * @brief Release a request and everything it owns.
 *
 * @param[in] request Request to destroy; may be NULL.
 */
void destroy_request(request_t *request);

/**
 * @brief Return the request method.
 *
 * @param[in] request Request to inspect; may be NULL.
 * @return A borrowed method string, or NULL if @p request is NULL.
 */
const char *request_get_method(const request_t *request);

/**
 * @brief Return the parsed request-target.
 *
 * @param[in] request Request to inspect; may be NULL.
 * @return A borrowed URL, or NULL if @p request is NULL.
 */
const url_t *request_get_url(const request_t *request);

/**
 * @brief Return the HTTP version.
 *
 * @param[in] request Request to inspect; may be NULL.
 * @return A borrowed version string, or NULL if @p request is NULL.
 */
const char *request_get_version(const request_t *request);

/**
 * @brief Return the mutable header map.
 *
 * Use the headers_* functions to add, replace or remove fields.
 *
 * @param[in] request Request to inspect; may be NULL.
 * @return A borrowed header map, or NULL if @p request is NULL.
 */
headers_t *request_get_headers(request_t *request);

/**
 * @brief Return the body bytes.
 *
 * @param[in]  request Request to inspect; may be NULL.
 * @param[out] len     Receives the body length; may be NULL.
 * @return A borrowed body pointer (NULL when empty), or NULL if @p request is
 *         NULL.
 */
const char *request_get_body(const request_t *request, size_t *len);

/**
 * @brief Set the method.
 *
 * @param[in,out] request Request to modify.
 * @param[in]     method  Non-empty token, e.g. "POST".
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int request_set_method(request_t *request, const char *method);

/**
 * @brief Set the request-target, parsing it into its components.
 *
 * @param[in,out] request Request to modify.
 * @param[in]     target  Request-target, e.g. "/a/b?x=1" or an absolute URL.
 * @return 0 on success, -1 on invalid arguments or malformed target.
 */
int request_set_url(request_t *request, const char *target);

/**
 * @brief Set the HTTP version.
 *
 * @param[in,out] request Request to modify.
 * @param[in]     version Version of the form "HTTP/<major>.<minor>".
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int request_set_version(request_t *request, const char *version);

/**
 * @brief Replace the body with a copy of @p body.
 *
 * @param[in,out] request Request to modify.
 * @param[in]     body    Bytes to copy; may be NULL only when @p len is 0.
 * @param[in]     len     Number of bytes.
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int request_set_body(request_t *request, const char *body, size_t len);

/**
 * @brief Parse a complete request from @p data.
 *
 * @param[in] data Wire bytes. May contain NUL bytes in the body.
 * @param[in] len  Number of bytes in @p data.
 * @return A new request, or NULL if @p data is NULL or malformed. Release it
 *         with destroy_request().
 */
request_t *request_parse(const char *data, size_t len);

/**
 * @brief Serialize a request to the wire format.
 *
 * The method, request-target, version, headers and body are rendered in order.
 * Framing headers are never added or corrected; if Transfer-Encoding contains
 * "chunked" the body is re-encoded as a single chunk.
 *
 * @param[in]  request Request to serialize.
 * @param[out] len     Receives the byte length; may be NULL. The result is also
 *                     NUL-terminated, but may contain embedded NULs.
 * @return A newly allocated string the caller must free(), or NULL on invalid
 *         arguments or allocation failure.
 */
char *request_to_string(const request_t *request, size_t *len);

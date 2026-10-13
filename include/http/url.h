#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @file url.h
 * @brief RFC 3986 URI-reference parser.
 *
 * A URL is split into the seven components defined by RFC 3986 section 3:
 * scheme, userinfo, host, port, path, query and fragment. Parsing is lenient
 * about which components are present (relative references are supported), but
 * each component that *is* present is validated against the grammar in
 * Appendix A.
 *
 * Percent-encoding is preserved verbatim: components are stored exactly as
 * they appeared in the input and url_to_string() round-trips losslessly. Only
 * the scheme and host are case-normalized (lowercased), as the RFC requires
 * them to be case-insensitive.
 *
 * @code
 * url_t *url = url_parse("https://user@example.com:8443/a/b?x=1#frag");
 * if (url)
 * {
 *     printf("%s://%s\n", url->scheme, url->host);
 *     url_destroy(url);
 * }
 * @endcode
 */

/**
 * @brief The components of a parsed URI reference.
 *
 * Every member that is not required by the input is NULL; this distinguishes an
 * absent component from an empty one. For example "http://h/p?#" yields
 * query == "" and fragment == "", while "http://h/p" yields query == NULL and
 * fragment == NULL.
 *
 * @note Treat instances as opaque; create them with url_parse() and release
 *       them with url_destroy(). All members are owned by the url_t.
 */
typedef struct url_s
{
    char *scheme;   /**< Lowercased scheme without ':'; NULL if absent. */
    char *userinfo; /**< Userinfo without '@'; NULL if absent. */
    char *host;     /**< Lowercased reg-name, or IP literal with brackets
                         stripped; NULL if there is no authority. */
    char *port;     /**< Port digits as text; NULL if absent. */
    char *path;     /**< Raw path; "" when empty. Never NULL. */
    char *query;    /**< Query without '?'; NULL if absent, "" if empty. */
    char *fragment; /**< Fragment without '#'; NULL if absent, "" if empty. */
} url_t;

/**
 * @brief Parse a URI reference into its components.
 *
 * @param[in] text NUL-terminated URI reference. Must not contain raw spaces,
 *                 C0 control characters or DEL.
 * @return A newly allocated url_t, or NULL if @p text is NULL or malformed.
 *         Release it with url_destroy().
 */
url_t *url_parse(const char *text);

/**
 * @brief Release a parsed URL and every component it owns.
 *
 * @param[in] url URL to destroy; may be NULL.
 */
void url_destroy(url_t *url);

/**
 * @brief Test whether a URL carries a scheme.
 *
 * @param[in] url URL to inspect; may be NULL.
 * @return true if a scheme is present, false otherwise.
 */
bool url_is_absolute(const url_t *url);

/**
 * @brief Return the default port for a URL's scheme.
 *
 * @param[in] url URL to inspect; may be NULL.
 * @return 80 for http/ws, 443 for https/wss, 21 for ftp, or -1 when the scheme
 *         is absent or has no well-known default.
 */
int url_default_port(const url_t *url);

/**
 * @brief Serialize a URL back into a string.
 *
 * @param[in] url URL to serialize; may be NULL.
 * @return A newly allocated, NUL-terminated string that the caller must free(),
 *         or NULL on invalid arguments or allocation failure.
 */
char *url_to_string(const url_t *url);

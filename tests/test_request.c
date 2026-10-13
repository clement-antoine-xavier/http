#include "unit.h"

#include "http/request.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void check_round_trip(const char *wire, size_t len)
{
    request_t *request = request_parse(wire, len);
    REQUIRE(request != NULL);
    size_t out_len = 0;
    char *out = request_to_string(request, &out_len);
    CHECK_NOT_NULL(out);
    if (out)
    {
        CHECK_INT_EQ(out_len, len);
        CHECK_INT_EQ(memcmp(out, wire, len), 0);
        free(out);
    }
    destroy_request(request);
}

static int body_is(const request_t *request, const char *expected, size_t len)
{
    size_t got_len = 0;
    const char *got = request_get_body(request, &got_len);
    return got_len == len && (len == 0 || memcmp(got, expected, len) == 0);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

TEST(create_returns_defaults)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_STR_EQ(request_get_method(request), "GET");
    CHECK_STR_EQ(request_get_version(request), "HTTP/1.1");
    CHECK_NOT_NULL(request_get_url(request));
    CHECK_STR_EQ(request_get_url(request)->path, "/");
    CHECK_INT_EQ(headers_count(request_get_headers(request)), 0);
    CHECK_NULL(request_get_body(request, NULL));
    destroy_request(request);
}

TEST(destroy_null_is_safe)
{
    destroy_request(NULL);
}

/* ------------------------------------------------------------------------- */
/* Setters / getters                                                         */
/* ------------------------------------------------------------------------- */

TEST(set_method_accepts_tokens)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_INT_EQ(request_set_method(request, "POST"), 0);
    CHECK_STR_EQ(request_get_method(request), "POST");
    destroy_request(request);
}

TEST(set_method_rejects_invalid_tokens)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_INT_EQ(request_set_method(request, NULL), -1);
    CHECK_INT_EQ(request_set_method(request, ""), -1);
    CHECK_INT_EQ(request_set_method(request, "GE T"), -1);
    CHECK_STR_EQ(request_get_method(request), "GET");
    destroy_request(request);
}

TEST(set_url_parses_target)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_INT_EQ(request_set_url(request, "/a/b?x=1"), 0);
    const url_t *url = request_get_url(request);
    CHECK_STR_EQ(url->path, "/a/b");
    CHECK_STR_EQ(url->query, "x=1");
    CHECK_INT_EQ(request_set_url(request, "not a url"), -1);
    destroy_request(request);
}

TEST(set_version_requires_http_form)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_INT_EQ(request_set_version(request, "HTTP/1.0"), 0);
    CHECK_STR_EQ(request_get_version(request), "HTTP/1.0");
    CHECK_INT_EQ(request_set_version(request, "HTTP/1"), -1);
    CHECK_INT_EQ(request_set_version(request, "1.1"), -1);
    CHECK_INT_EQ(request_set_version(request, NULL), -1);
    destroy_request(request);
}

TEST(set_body_is_binary_safe_and_clearable)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    const char payload[] = {'a', '\0', 'b'};
    CHECK_INT_EQ(request_set_body(request, payload, sizeof(payload)), 0);
    CHECK_TRUE(body_is(request, payload, sizeof(payload)));

    CHECK_INT_EQ(request_set_body(request, NULL, 0), 0);
    CHECK_NULL(request_get_body(request, NULL));
    CHECK_INT_EQ(request_set_body(request, NULL, 3), -1);
    destroy_request(request);
}

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

TEST(parse_simple_get)
{
    const char *wire = "GET / HTTP/1.1\r\n\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_STR_EQ(request_get_method(request), "GET");
    CHECK_STR_EQ(request_get_version(request), "HTTP/1.1");
    CHECK_STR_EQ(request_get_url(request)->path, "/");
    CHECK_INT_EQ(headers_count(request_get_headers(request)), 0);
    CHECK_NULL(request_get_body(request, NULL));
    destroy_request(request);
}

TEST(parse_http_1_0)
{
    const char *wire = "GET / HTTP/1.0\r\n\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_STR_EQ(request_get_version(request), "HTTP/1.0");
    destroy_request(request);
}

TEST(parse_star_form_target)
{
    const char *wire = "OPTIONS * HTTP/1.1\r\n\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_STR_EQ(request_get_url(request)->path, "*");
    destroy_request(request);
}

TEST(parse_absolute_form_target)
{
    const char *wire = "GET http://example.com/a?b=1 HTTP/1.1\r\n\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_STR_EQ(request_get_url(request)->host, "example.com");
    CHECK_STR_EQ(request_get_url(request)->path, "/a");
    CHECK_STR_EQ(request_get_url(request)->query, "b=1");
    destroy_request(request);
}

TEST(parse_content_length_body)
{
    const char *wire =
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_STR_EQ(headers_get(request_get_headers(request), "host"), "example.com");
    CHECK_TRUE(body_is(request, "hello world", 11));
    destroy_request(request);
}

TEST(parse_binary_body)
{
    const char wire[] =
        "POST / HTTP/1.1\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "a\0b";
    request_t *request = request_parse(wire, sizeof(wire) - 1);
    REQUIRE(request != NULL);
    const char expected[] = {'a', '\0', 'b'};
    CHECK_TRUE(body_is(request, expected, 3));
    destroy_request(request);
}

TEST(parse_repeated_header_values)
{
    const char *wire =
        "GET / HTTP/1.1\r\n"
        "X-Tag: a\r\n"
        "X-Tag: b\r\n"
        "\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_INT_EQ(headers_count(request_get_headers(request)), 2);
    destroy_request(request);
}

TEST(parse_trims_header_whitespace)
{
    const char *wire =
        "GET / HTTP/1.1\r\n"
        "X-Padded:   spaced value  \r\n"
        "\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_STR_EQ(headers_get(request_get_headers(request), "X-Padded"),
                 "spaced value");
    destroy_request(request);
}

/* ------------------------------------------------------------------------- */
/* Chunked transfer coding                                                   */
/* ------------------------------------------------------------------------- */

TEST(parse_chunked_body)
{
    const char *wire =
        "POST / HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "6\r\n world\r\n"
        "0\r\n"
        "\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_TRUE(body_is(request, "hello world", 11));
    destroy_request(request);
}

TEST(parse_chunk_extensions_and_trailers)
{
    const char *wire =
        "POST / HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4;name=value\r\nWiki\r\n"
        "0\r\n"
        "X-Trailer: ignored\r\n"
        "\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_TRUE(body_is(request, "Wiki", 4));
    destroy_request(request);
}

TEST(parse_empty_chunked_body)
{
    const char *wire =
        "POST / HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_NULL(request_get_body(request, NULL));
    destroy_request(request);
}

TEST(parse_chunked_takes_precedence_over_content_length)
{
    const char *wire =
        "POST / HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Length: 99\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "0\r\n"
        "\r\n";
    request_t *request = request_parse(wire, strlen(wire));
    REQUIRE(request != NULL);
    CHECK_TRUE(body_is(request, "hello", 5));
    destroy_request(request);
}

TEST(parse_errors_return_null)
{
    const char *no_crlf = "GET / HTTP/1.1";
    CHECK_NULL(request_parse(no_crlf, strlen(no_crlf)));

    const char *missing_target = "GET\r\n\r\n";
    CHECK_NULL(request_parse(missing_target, strlen(missing_target)));

    const char *empty_method = " / HTTP/1.1\r\n\r\n";
    CHECK_NULL(request_parse(empty_method, strlen(empty_method)));

    const char *bad_version = "GET / HTTP/1.1x\r\n\r\n";
    CHECK_NULL(request_parse(bad_version, strlen(bad_version)));

    const char *bad_header = "GET / HTTP/1.1\r\nNoColon\r\n\r\n";
    CHECK_NULL(request_parse(bad_header, strlen(bad_header)));

    const char *no_blank_line = "GET / HTTP/1.1\r\nHost: x";
    CHECK_NULL(request_parse(no_blank_line, strlen(no_blank_line)));

    const char *truncated =
        "POST / HTTP/1.1\r\nContent-Length: 10\r\n\r\nshort";
    CHECK_NULL(request_parse(truncated, strlen(truncated)));

    const char *bad_chunk =
        "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n";
    CHECK_NULL(request_parse(bad_chunk, strlen(bad_chunk)));

    const char *unterminated_chunk =
        "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n";
    CHECK_NULL(request_parse(unterminated_chunk, strlen(unterminated_chunk)));

    CHECK_NULL(request_parse(NULL, 0));
}

/* ------------------------------------------------------------------------- */
/* Serialization                                                             */
/* ------------------------------------------------------------------------- */

TEST(to_string_emits_exact_wire_format)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_INT_EQ(request_set_method(request, "GET"), 0);
    CHECK_INT_EQ(request_set_url(request, "/index.html"), 0);
    CHECK_INT_EQ(headers_set(request_get_headers(request), "Host", "example.com"), 0);

    size_t len = 0;
    char *out = request_to_string(request, &len);
    REQUIRE(out != NULL);
    const char *expected = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n";
    CHECK_INT_EQ(len, strlen(expected));
    CHECK_INT_EQ(memcmp(out, expected, len), 0);
    free(out);
    destroy_request(request);
}

TEST(round_trip_preserves_message)
{
    check_round_trip("GET / HTTP/1.1\r\n\r\n", strlen("GET / HTTP/1.1\r\n\r\n"));
    check_round_trip("GET /a?b=1 HTTP/1.0\r\nHost: h\r\n\r\n",
                     strlen("GET /a?b=1 HTTP/1.0\r\nHost: h\r\n\r\n"));
}

TEST(chunked_round_trip)
{
    request_t *request = create_request();
    REQUIRE(request != NULL);
    CHECK_INT_EQ(request_set_method(request, "POST"), 0);
    CHECK_INT_EQ(headers_set(request_get_headers(request), "Transfer-Encoding",
                             "chunked"),
                 0);
    CHECK_INT_EQ(request_set_body(request, "hello", 5), 0);

    size_t len = 0;
    char *out = request_to_string(request, &len);
    REQUIRE(out != NULL);
    const char *expected =
        "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n";
    CHECK_INT_EQ(len, strlen(expected));
    CHECK_INT_EQ(memcmp(out, expected, len), 0);

    request_t *reparsed = request_parse(out, len);
    REQUIRE(reparsed != NULL);
    CHECK_TRUE(body_is(reparsed, "hello", 5));

    free(out);
    destroy_request(reparsed);
    destroy_request(request);
}

TEST(serialize_invalid_arguments)
{
    size_t len = 123;
    CHECK_NULL(request_to_string(NULL, &len));
    CHECK_INT_EQ(len, 0);
}

TEST(getters_on_null_are_safe)
{
    CHECK_NULL(request_get_method(NULL));
    CHECK_NULL(request_get_url(NULL));
    CHECK_NULL(request_get_version(NULL));
    CHECK_NULL(request_get_headers(NULL));
    CHECK_NULL(request_get_body(NULL, NULL));
}

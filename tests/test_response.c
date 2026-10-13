#include "unit.h"

#include "http/response.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void check_round_trip(const char *wire, size_t len)
{
    response_t *response = response_parse(wire, len);
    REQUIRE(response != NULL);
    size_t out_len = 0;
    char *out = response_to_string(response, &out_len);
    CHECK_NOT_NULL(out);
    if (out)
    {
        CHECK_INT_EQ(out_len, len);
        CHECK_INT_EQ(memcmp(out, wire, len), 0);
        free(out);
    }
    destroy_response(response);
}

static int body_is(const response_t *response, const char *expected, size_t len)
{
    size_t got_len = 0;
    const char *got = response_get_body(response, &got_len);
    return got_len == len && (len == 0 || memcmp(got, expected, len) == 0);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

TEST(create_returns_defaults)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    CHECK_STR_EQ(response_get_version(response), "HTTP/1.1");
    CHECK_INT_EQ(response_get_status(response), 200);
    CHECK_NULL(response_get_reason(response));
    CHECK_INT_EQ(headers_count(response_get_headers(response)), 0);
    CHECK_NULL(response_get_body(response, NULL));
    destroy_response(response);
}

TEST(destroy_null_is_safe)
{
    destroy_response(NULL);
}

/* ------------------------------------------------------------------------- */
/* Setters / getters                                                         */
/* ------------------------------------------------------------------------- */

TEST(set_version_requires_http_form)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    CHECK_INT_EQ(response_set_version(response, "HTTP/1.0"), 0);
    CHECK_STR_EQ(response_get_version(response), "HTTP/1.0");
    CHECK_INT_EQ(response_set_version(response, "HTTP/1"), -1);
    CHECK_INT_EQ(response_set_version(response, NULL), -1);
    destroy_response(response);
}

TEST(set_status_range_is_enforced)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    CHECK_INT_EQ(response_set_status(response, 404), 0);
    CHECK_INT_EQ(response_get_status(response), 404);
    CHECK_INT_EQ(response_set_status(response, 99), -1);
    CHECK_INT_EQ(response_set_status(response, 1000), -1);
    CHECK_INT_EQ(response_get_status(response), 404);
    destroy_response(response);
}

TEST(set_reason_and_clear)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    CHECK_INT_EQ(response_set_reason(response, "Not Found"), 0);
    CHECK_STR_EQ(response_get_reason(response), "Not Found");
    CHECK_INT_EQ(response_set_reason(response, "Bad\r\nInjected"), -1);
    CHECK_STR_EQ(response_get_reason(response), "Not Found");
    CHECK_INT_EQ(response_set_reason(response, NULL), 0);
    CHECK_NULL(response_get_reason(response));
    destroy_response(response);
}

TEST(set_body_is_binary_safe_and_clearable)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    const char payload[] = {'x', '\0', 'y'};
    CHECK_INT_EQ(response_set_body(response, payload, sizeof(payload)), 0);
    CHECK_TRUE(body_is(response, payload, sizeof(payload)));

    CHECK_INT_EQ(response_set_body(response, NULL, 0), 0);
    CHECK_NULL(response_get_body(response, NULL));
    CHECK_INT_EQ(response_set_body(response, NULL, 3), -1);
    destroy_response(response);
}

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

TEST(parse_status_with_reason)
{
    const char *wire = "HTTP/1.1 404 Not Found\r\n\r\n";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_STR_EQ(response_get_version(response), "HTTP/1.1");
    CHECK_INT_EQ(response_get_status(response), 404);
    CHECK_STR_EQ(response_get_reason(response), "Not Found");
    destroy_response(response);
}

TEST(parse_reason_is_optional)
{
    const char *wire = "HTTP/1.1 200\r\n\r\n";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_INT_EQ(response_get_status(response), 200);
    CHECK_NULL(response_get_reason(response));
    destroy_response(response);
}

TEST(parse_empty_reason_phrase)
{
    const char *wire = "HTTP/1.1 204 \r\n\r\n";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_INT_EQ(response_get_status(response), 204);
    CHECK_NULL(response_get_reason(response));
    destroy_response(response);
}

TEST(parse_content_length_body)
{
    const char *wire =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_STR_EQ(headers_get(response_get_headers(response), "content-type"),
                 "text/plain");
    CHECK_TRUE(body_is(response, "hello", 5));
    destroy_response(response);
}

TEST(parse_body_without_content_length_takes_remainder)
{
    const char *wire = "HTTP/1.1 200 OK\r\n\r\nrest of body";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_TRUE(body_is(response, "rest of body", 12));
    destroy_response(response);
}

TEST(parse_binary_body)
{
    const char wire[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "a\0b";
    response_t *response = response_parse(wire, sizeof(wire) - 1);
    REQUIRE(response != NULL);
    const char expected[] = {'a', '\0', 'b'};
    CHECK_TRUE(body_is(response, expected, 3));
    destroy_response(response);
}

/* ------------------------------------------------------------------------- */
/* Chunked transfer coding                                                   */
/* ------------------------------------------------------------------------- */

TEST(parse_chunked_body)
{
    const char *wire =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "6\r\n world\r\n"
        "0\r\n"
        "\r\n";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_TRUE(body_is(response, "hello world", 11));
    destroy_response(response);
}

TEST(parse_chunk_extensions_and_trailers)
{
    const char *wire =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4;ext=1\r\nWiki\r\n"
        "0\r\n"
        "X-Trailer: ignored\r\n"
        "\r\n";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_TRUE(body_is(response, "Wiki", 4));
    destroy_response(response);
}

TEST(parse_chunked_takes_precedence_over_content_length)
{
    const char *wire =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Length: 99\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "0\r\n"
        "\r\n";
    response_t *response = response_parse(wire, strlen(wire));
    REQUIRE(response != NULL);
    CHECK_TRUE(body_is(response, "hello", 5));
    destroy_response(response);
}

TEST(parse_errors_return_null)
{
    const char *no_crlf = "HTTP/1.1 200 OK";
    CHECK_NULL(response_parse(no_crlf, strlen(no_crlf)));

    const char *bad_version = "FTP/1.1 200 OK\r\n\r\n";
    CHECK_NULL(response_parse(bad_version, strlen(bad_version)));

    const char *short_status = "HTTP/1.1 20 OK\r\n\r\n";
    CHECK_NULL(response_parse(short_status, strlen(short_status)));

    const char *non_digit_status = "HTTP/1.1 2O0 OK\r\n\r\n";
    CHECK_NULL(response_parse(non_digit_status, strlen(non_digit_status)));

    const char *missing_sp = "HTTP/1.1200 OK\r\n\r\n";
    CHECK_NULL(response_parse(missing_sp, strlen(missing_sp)));

    const char *bad_header = "HTTP/1.1 200 OK\r\nNoColon\r\n\r\n";
    CHECK_NULL(response_parse(bad_header, strlen(bad_header)));

    const char *truncated =
        "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort";
    CHECK_NULL(response_parse(truncated, strlen(truncated)));

    const char *unterminated_chunk =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n";
    CHECK_NULL(response_parse(unterminated_chunk, strlen(unterminated_chunk)));

    CHECK_NULL(response_parse(NULL, 0));
}

/* ------------------------------------------------------------------------- */
/* Serialization                                                             */
/* ------------------------------------------------------------------------- */

TEST(to_string_emits_exact_wire_format)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    CHECK_INT_EQ(response_set_status(response, 404), 0);
    CHECK_INT_EQ(response_set_reason(response, "Not Found"), 0);
    CHECK_INT_EQ(headers_set(response_get_headers(response), "Content-Length", "0"),
                 0);

    size_t len = 0;
    char *out = response_to_string(response, &len);
    REQUIRE(out != NULL);
    const char *expected =
        "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    CHECK_INT_EQ(len, strlen(expected));
    CHECK_INT_EQ(memcmp(out, expected, len), 0);
    free(out);
    destroy_response(response);
}

TEST(to_string_without_reason_emits_trailing_space)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    size_t len = 0;
    char *out = response_to_string(response, &len);
    REQUIRE(out != NULL);
    const char *expected = "HTTP/1.1 200 \r\n\r\n";
    CHECK_INT_EQ(len, strlen(expected));
    CHECK_INT_EQ(memcmp(out, expected, len), 0);
    free(out);
    destroy_response(response);
}

TEST(round_trip_preserves_message)
{
    check_round_trip("HTTP/1.1 200 OK\r\n\r\n", strlen("HTTP/1.1 200 OK\r\n\r\n"));
    check_round_trip("HTTP/1.0 404 Not Found\r\nX-A: b\r\n\r\n",
                     strlen("HTTP/1.0 404 Not Found\r\nX-A: b\r\n\r\n"));
    check_round_trip("HTTP/1.1 204 \r\n\r\n", strlen("HTTP/1.1 204 \r\n\r\n"));
}

TEST(chunked_round_trip)
{
    response_t *response = create_response();
    REQUIRE(response != NULL);
    CHECK_INT_EQ(headers_set(response_get_headers(response), "Transfer-Encoding",
                             "chunked"),
                 0);
    CHECK_INT_EQ(response_set_body(response, "hello", 5), 0);

    size_t len = 0;
    char *out = response_to_string(response, &len);
    REQUIRE(out != NULL);
    const char *expected =
        "HTTP/1.1 200 \r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n";
    CHECK_INT_EQ(len, strlen(expected));
    CHECK_INT_EQ(memcmp(out, expected, len), 0);

    response_t *reparsed = response_parse(out, len);
    REQUIRE(reparsed != NULL);
    CHECK_TRUE(body_is(reparsed, "hello", 5));

    free(out);
    destroy_response(reparsed);
    destroy_response(response);
}

TEST(serialize_invalid_arguments)
{
    size_t len = 123;
    CHECK_NULL(response_to_string(NULL, &len));
    CHECK_INT_EQ(len, 0);
}

TEST(getters_on_null_are_safe)
{
    CHECK_NULL(response_get_version(NULL));
    CHECK_INT_EQ(response_get_status(NULL), 0);
    CHECK_NULL(response_get_reason(NULL));
    CHECK_NULL(response_get_headers(NULL));
    CHECK_NULL(response_get_body(NULL, NULL));
}

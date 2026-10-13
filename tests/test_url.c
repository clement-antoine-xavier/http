#include "unit.h"

#include "http/url.h"

#include <stdlib.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void check_round_trip(const char *text)
{
    url_t *url = url_parse(text);
    REQUIRE(url != NULL);
    char *out = url_to_string(url);
    CHECK_NOT_NULL(out);
    if (out)
    {
        CHECK_STR_EQ(out, text);
        free(out);
    }
    url_destroy(url);
}

/* ------------------------------------------------------------------------- */
/* Component parsing                                                         */
/* ------------------------------------------------------------------------- */

TEST(parse_full_url)
{
    url_t *url = url_parse("https://user:pass@example.com:8443/a/b?x=1#frag");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "https");
    CHECK_STR_EQ(url->userinfo, "user:pass");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->port, "8443");
    CHECK_STR_EQ(url->path, "/a/b");
    CHECK_STR_EQ(url->query, "x=1");
    CHECK_STR_EQ(url->fragment, "frag");
    CHECK_TRUE(url_is_absolute(url));
    url_destroy(url);
}

TEST(no_path_leaves_empty_path)
{
    url_t *url = url_parse("http://example.com");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "http");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->path, "");
    CHECK_NULL(url->userinfo);
    CHECK_NULL(url->port);
    CHECK_NULL(url->query);
    CHECK_NULL(url->fragment);
    CHECK_INT_EQ(url_default_port(url), 80);
    url_destroy(url);
}

TEST(userinfo_without_port)
{
    url_t *url = url_parse("http://user:secret@example.com/");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->userinfo, "user:secret");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_NULL(url->port);
    CHECK_STR_EQ(url->path, "/");
    url_destroy(url);
}

TEST(ipv6_literal_with_port)
{
    url_t *url = url_parse("http://[2001:db8::1]:8080/index.html");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->host, "2001:db8::1");
    CHECK_STR_EQ(url->port, "8080");
    CHECK_STR_EQ(url->path, "/index.html");
    url_destroy(url);
}

TEST(ipv6_literal_without_port)
{
    url_t *url = url_parse("http://[::1]/");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->host, "::1");
    CHECK_NULL(url->port);
    url_destroy(url);
}

TEST(empty_authority_host)
{
    url_t *url = url_parse("file:///etc/hosts");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "file");
    CHECK_NOT_NULL(url->host);
    CHECK_STR_EQ(url->host, "");
    CHECK_STR_EQ(url->path, "/etc/hosts");
    CHECK_NULL(url->port);
    url_destroy(url);
}

TEST(opaque_path_with_scheme_but_no_authority)
{
    url_t *url = url_parse("mailto:user@example.com");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "mailto");
    CHECK_NULL(url->host);
    CHECK_STR_EQ(url->path, "user@example.com");
    CHECK_TRUE(url_is_absolute(url));
    url_destroy(url);
}

/* ------------------------------------------------------------------------- */
/* Absent vs empty                                                           */
/* ------------------------------------------------------------------------- */

TEST(empty_query_and_fragment_are_present)
{
    url_t *url = url_parse("http://h/p?#");
    REQUIRE(url != NULL);
    CHECK_NOT_NULL(url->query);
    CHECK_STR_EQ(url->query, "");
    CHECK_NOT_NULL(url->fragment);
    CHECK_STR_EQ(url->fragment, "");
    url_destroy(url);
}

TEST(absent_query_and_fragment_are_null)
{
    url_t *url = url_parse("http://h/p");
    REQUIRE(url != NULL);
    CHECK_NULL(url->query);
    CHECK_NULL(url->fragment);
    url_destroy(url);
}

/* ------------------------------------------------------------------------- */
/* Normalization                                                             */
/* ------------------------------------------------------------------------- */

TEST(scheme_and_host_are_lowercased)
{
    url_t *url = url_parse("HTTP://EXAMPLE.COM/Path?Q=V#Frag");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "http");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->path, "/Path");
    CHECK_STR_EQ(url->query, "Q=V");
    CHECK_STR_EQ(url->fragment, "Frag");
    url_destroy(url);
}

/* ------------------------------------------------------------------------- */
/* Relative references                                                       */
/* ------------------------------------------------------------------------- */

TEST(relative_reference)
{
    url_t *url = url_parse("/a/b?x=1#f");
    REQUIRE(url != NULL);
    CHECK_NULL(url->scheme);
    CHECK_NULL(url->host);
    CHECK_FALSE(url_is_absolute(url));
    CHECK_STR_EQ(url->path, "/a/b");
    CHECK_STR_EQ(url->query, "x=1");
    CHECK_STR_EQ(url->fragment, "f");
    url_destroy(url);
}

TEST(network_path_reference)
{
    url_t *url = url_parse("//example.com/path");
    REQUIRE(url != NULL);
    CHECK_NULL(url->scheme);
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->path, "/path");
    CHECK_FALSE(url_is_absolute(url));
    url_destroy(url);
}

/* ------------------------------------------------------------------------- */
/* Default ports                                                             */
/* ------------------------------------------------------------------------- */

TEST(default_ports)
{
    url_t *http = url_parse("http://h/");
    url_t *https = url_parse("https://h/");
    url_t *ws = url_parse("ws://h/");
    url_t *ftp = url_parse("ftp://h/");
    url_t *other = url_parse("gopher://h/");
    REQUIRE(http != NULL);
    REQUIRE(https != NULL);
    REQUIRE(ws != NULL);
    REQUIRE(ftp != NULL);
    REQUIRE(other != NULL);

    CHECK_INT_EQ(url_default_port(http), 80);
    CHECK_INT_EQ(url_default_port(https), 443);
    CHECK_INT_EQ(url_default_port(ws), 80);
    CHECK_INT_EQ(url_default_port(ftp), 21);
    CHECK_INT_EQ(url_default_port(other), -1);
    CHECK_INT_EQ(url_default_port(NULL), -1);

    url_destroy(http);
    url_destroy(https);
    url_destroy(ws);
    url_destroy(ftp);
    url_destroy(other);
}

/* ------------------------------------------------------------------------- */
/* Invalid input                                                             */
/* ------------------------------------------------------------------------- */

TEST(reject_invalid_input)
{
    CHECK_NULL(url_parse(NULL));
    CHECK_NULL(url_parse("1http://example.com"));  /* scheme starts with digit */
    CHECK_NULL(url_parse("://example.com"));       /* empty scheme */
    CHECK_NULL(url_parse("ht tp://example.com"));  /* raw space */
    CHECK_NULL(url_parse("http://exa mple.com"));  /* raw space in host */
    CHECK_NULL(url_parse("http://host:abc/"));     /* non-digit port */
    CHECK_NULL(url_parse("http://host:99999/"));   /* port out of range */
    CHECK_NULL(url_parse("http://host:123456/"));  /* too many digits */
    CHECK_NULL(url_parse("http://[::1/"));         /* unterminated IPv6 literal */
}

/* ------------------------------------------------------------------------- */
/* NULL safety                                                               */
/* ------------------------------------------------------------------------- */

TEST(null_safety)
{
    url_destroy(NULL);
    CHECK_NULL(url_to_string(NULL));
    CHECK_FALSE(url_is_absolute(NULL));
}

/* ------------------------------------------------------------------------- */
/* Serialization round-trip                                                  */
/* ------------------------------------------------------------------------- */

TEST(round_trip_is_lossless)
{
    check_round_trip("https://user:pass@example.com:8443/a/b?x=1#frag");
    check_round_trip("http://example.com");
    check_round_trip("http://example.com/");
    check_round_trip("http://[::1]:8080/");
    check_round_trip("file:///etc/hosts");
    check_round_trip("http://h/p?#");
    check_round_trip("mailto:user@example.com");
    check_round_trip("/a/b?x=1#f");
    check_round_trip("//example.com/path");
}

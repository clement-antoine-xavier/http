#include "unit.h"

#include "http/url.h"

#include <stdlib.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void check_round_trip(const char *text)
{
    url_t *url = create_url(text);
    REQUIRE(url != NULL);
    char *out = url_to_string(url);
    CHECK_NOT_NULL(out);
    if (out)
    {
        CHECK_STR_EQ(out, text);
        free(out);
    }
    destroy_url(url);
}

/* ------------------------------------------------------------------------- */
/* Component parsing                                                         */
/* ------------------------------------------------------------------------- */

TEST(parse_full_url)
{
    url_t *url = create_url("https://user:pass@example.com:8443/a/b?x=1#frag");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "https");
    CHECK_STR_EQ(url->userinfo, "user:pass");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->port, "8443");
    CHECK_STR_EQ(url->path, "/a/b");
    CHECK_STR_EQ(url->query, "x=1");
    CHECK_STR_EQ(url->fragment, "frag");
    CHECK_TRUE(url_is_absolute(url));
    destroy_url(url);
}

TEST(no_path_leaves_empty_path)
{
    url_t *url = create_url("http://example.com");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "http");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->path, "");
    CHECK_NULL(url->userinfo);
    CHECK_NULL(url->port);
    CHECK_NULL(url->query);
    CHECK_NULL(url->fragment);
    CHECK_INT_EQ(url_default_port(url), 80);
    destroy_url(url);
}

TEST(userinfo_without_port)
{
    url_t *url = create_url("http://user:secret@example.com/");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->userinfo, "user:secret");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_NULL(url->port);
    CHECK_STR_EQ(url->path, "/");
    destroy_url(url);
}

TEST(ipv6_literal_with_port)
{
    url_t *url = create_url("http://[2001:db8::1]:8080/index.html");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->host, "2001:db8::1");
    CHECK_STR_EQ(url->port, "8080");
    CHECK_STR_EQ(url->path, "/index.html");
    destroy_url(url);
}

TEST(ipv6_literal_without_port)
{
    url_t *url = create_url("http://[::1]/");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->host, "::1");
    CHECK_NULL(url->port);
    destroy_url(url);
}

TEST(empty_authority_host)
{
    url_t *url = create_url("file:///etc/hosts");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "file");
    CHECK_NOT_NULL(url->host);
    CHECK_STR_EQ(url->host, "");
    CHECK_STR_EQ(url->path, "/etc/hosts");
    CHECK_NULL(url->port);
    destroy_url(url);
}

TEST(opaque_path_with_scheme_but_no_authority)
{
    url_t *url = create_url("mailto:user@example.com");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "mailto");
    CHECK_NULL(url->host);
    CHECK_STR_EQ(url->path, "user@example.com");
    CHECK_TRUE(url_is_absolute(url));
    destroy_url(url);
}

/* ------------------------------------------------------------------------- */
/* Absent vs empty                                                           */
/* ------------------------------------------------------------------------- */

TEST(empty_query_and_fragment_are_present)
{
    url_t *url = create_url("http://h/p?#");
    REQUIRE(url != NULL);
    CHECK_NOT_NULL(url->query);
    CHECK_STR_EQ(url->query, "");
    CHECK_NOT_NULL(url->fragment);
    CHECK_STR_EQ(url->fragment, "");
    destroy_url(url);
}

TEST(absent_query_and_fragment_are_null)
{
    url_t *url = create_url("http://h/p");
    REQUIRE(url != NULL);
    CHECK_NULL(url->query);
    CHECK_NULL(url->fragment);
    destroy_url(url);
}

/* ------------------------------------------------------------------------- */
/* Normalization                                                             */
/* ------------------------------------------------------------------------- */

TEST(scheme_and_host_are_lowercased)
{
    url_t *url = create_url("HTTP://EXAMPLE.COM/Path?Q=V#Frag");
    REQUIRE(url != NULL);
    CHECK_STR_EQ(url->scheme, "http");
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->path, "/Path");
    CHECK_STR_EQ(url->query, "Q=V");
    CHECK_STR_EQ(url->fragment, "Frag");
    destroy_url(url);
}

/* ------------------------------------------------------------------------- */
/* Relative references                                                       */
/* ------------------------------------------------------------------------- */

TEST(relative_reference)
{
    url_t *url = create_url("/a/b?x=1#f");
    REQUIRE(url != NULL);
    CHECK_NULL(url->scheme);
    CHECK_NULL(url->host);
    CHECK_FALSE(url_is_absolute(url));
    CHECK_STR_EQ(url->path, "/a/b");
    CHECK_STR_EQ(url->query, "x=1");
    CHECK_STR_EQ(url->fragment, "f");
    destroy_url(url);
}

TEST(network_path_reference)
{
    url_t *url = create_url("//example.com/path");
    REQUIRE(url != NULL);
    CHECK_NULL(url->scheme);
    CHECK_STR_EQ(url->host, "example.com");
    CHECK_STR_EQ(url->path, "/path");
    CHECK_FALSE(url_is_absolute(url));
    destroy_url(url);
}

/* ------------------------------------------------------------------------- */
/* Default ports                                                             */
/* ------------------------------------------------------------------------- */

TEST(default_ports)
{
    url_t *http = create_url("http://h/");
    url_t *https = create_url("https://h/");
    url_t *ws = create_url("ws://h/");
    url_t *ftp = create_url("ftp://h/");
    url_t *other = create_url("gopher://h/");
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

    destroy_url(http);
    destroy_url(https);
    destroy_url(ws);
    destroy_url(ftp);
    destroy_url(other);
}

/* ------------------------------------------------------------------------- */
/* Invalid input                                                             */
/* ------------------------------------------------------------------------- */

TEST(reject_invalid_input)
{
    CHECK_NULL(create_url(NULL));
    CHECK_NULL(create_url("1http://example.com"));  /* scheme starts with digit */
    CHECK_NULL(create_url("://example.com"));       /* empty scheme */
    CHECK_NULL(create_url("ht tp://example.com"));  /* raw space */
    CHECK_NULL(create_url("http://exa mple.com"));  /* raw space in host */
    CHECK_NULL(create_url("http://host:abc/"));     /* non-digit port */
    CHECK_NULL(create_url("http://host:99999/"));   /* port out of range */
    CHECK_NULL(create_url("http://host:123456/"));  /* too many digits */
    CHECK_NULL(create_url("http://[::1/"));         /* unterminated IPv6 literal */
}

/* ------------------------------------------------------------------------- */
/* NULL safety                                                               */
/* ------------------------------------------------------------------------- */

TEST(null_safety)
{
    destroy_url(NULL);
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

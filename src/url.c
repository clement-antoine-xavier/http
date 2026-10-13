#include "http/url.h"

#include <stdlib.h>
#include <string.h>

static char *dup_range(const char *start, size_t len)
{
    char *copy = malloc(len + 1);
    if (!copy)
        return NULL;
    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static void ascii_lower(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s + ('a' - 'A'));
}

static int valid_scheme(const char *s, size_t len)
{
    if (len == 0)
        return 0;
    if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')))
        return 0;
    for (size_t i = 1; i < len; i++)
    {
        char c = s[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
        if (!ok)
            return 0;
    }
    return 1;
}

static int valid_port(const char *s, size_t len)
{
    if (len == 0)
        return 1;
    if (len > 5)
        return 0;
    unsigned long value = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        value = value * 10 + (unsigned long)(s[i] - '0');
    }
    return value <= 65535;
}

static int valid_text(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (*p <= 0x20 || *p == 0x7f)
            return 0;
    return 1;
}

url_t *url_parse(const char *text)
{
    if (!text || !valid_text(text))
        return NULL;

    const char *p = text;
    const char *scheme_start = NULL;
    size_t scheme_len = 0;

    const char *colon = NULL;
    for (const char *q = p; *q; q++)
    {
        if (*q == ':')
        {
            colon = q;
            break;
        }
        if (*q == '/' || *q == '?' || *q == '#')
            break;
    }
    if (colon)
    {
        scheme_len = (size_t)(colon - p);
        if (!valid_scheme(p, scheme_len))
            return NULL;
        scheme_start = p;
        p = colon + 1;
    }

    int has_authority = 0;
    const char *userinfo_start = NULL;
    size_t userinfo_len = 0;
    const char *host_start = NULL;
    size_t host_len = 0;
    const char *port_start = NULL;
    size_t port_len = 0;

    if (p[0] == '/' && p[1] == '/')
    {
        has_authority = 1;
        p += 2;
        const char *auth = p;
        while (*p && *p != '/' && *p != '?' && *p != '#')
            p++;
        const char *auth_end = p;
        size_t auth_len = (size_t)(auth_end - auth);

        const char *hostport = auth;
        size_t hostport_len = auth_len;
        const char *at = NULL;
        for (size_t i = 0; i < auth_len; i++)
            if (auth[i] == '@')
                at = auth + i;
        if (at)
        {
            userinfo_start = auth;
            userinfo_len = (size_t)(at - auth);
            hostport = at + 1;
            hostport_len = (size_t)(auth_end - hostport);
        }

        if (hostport_len > 0 && hostport[0] == '[')
        {
            const char *close = NULL;
            for (size_t i = 1; i < hostport_len; i++)
                if (hostport[i] == ']')
                {
                    close = hostport + i;
                    break;
                }
            if (!close)
                return NULL;
            host_start = hostport + 1;
            host_len = (size_t)(close - host_start);
            size_t rest_len = (size_t)(hostport + hostport_len - (close + 1));
            if (rest_len > 0)
            {
                if (close[1] != ':')
                    return NULL;
                port_start = close + 2;
                port_len = rest_len - 1;
                if (!valid_port(port_start, port_len))
                    return NULL;
            }
        }
        else
        {
            const char *sep = NULL;
            for (size_t i = 0; i < hostport_len; i++)
                if (hostport[i] == ':')
                {
                    sep = hostport + i;
                    break;
                }
            if (sep)
            {
                host_start = hostport;
                host_len = (size_t)(sep - hostport);
                port_start = sep + 1;
                port_len = (size_t)(hostport + hostport_len - port_start);
                if (!valid_port(port_start, port_len))
                    return NULL;
            }
            else
            {
                host_start = hostport;
                host_len = hostport_len;
            }
        }
    }

    const char *path_start = p;
    while (*p && *p != '?' && *p != '#')
        p++;
    size_t path_len = (size_t)(p - path_start);

    int has_query = 0;
    const char *query_start = NULL;
    size_t query_len = 0;
    if (*p == '?')
    {
        p++;
        has_query = 1;
        query_start = p;
        while (*p && *p != '#')
            p++;
        query_len = (size_t)(p - query_start);
    }

    int has_fragment = 0;
    const char *fragment_start = NULL;
    size_t fragment_len = 0;
    if (*p == '#')
    {
        p++;
        has_fragment = 1;
        fragment_start = p;
        fragment_len = strlen(fragment_start);
    }

    url_t *url = calloc(1, sizeof(*url));
    if (!url)
        return NULL;

    if (scheme_start)
    {
        url->scheme = dup_range(scheme_start, scheme_len);
        if (!url->scheme)
            goto fail;
        ascii_lower(url->scheme);
    }
    if (userinfo_start)
    {
        url->userinfo = dup_range(userinfo_start, userinfo_len);
        if (!url->userinfo)
            goto fail;
    }
    if (has_authority)
    {
        url->host = dup_range(host_start, host_len);
        if (!url->host)
            goto fail;
        ascii_lower(url->host);
        if (port_len > 0)
        {
            url->port = dup_range(port_start, port_len);
            if (!url->port)
                goto fail;
        }
    }
    url->path = dup_range(path_start, path_len);
    if (!url->path)
        goto fail;
    if (has_query)
    {
        url->query = dup_range(query_start, query_len);
        if (!url->query)
            goto fail;
    }
    if (has_fragment)
    {
        url->fragment = dup_range(fragment_start, fragment_len);
        if (!url->fragment)
            goto fail;
    }
    return url;

fail:
    url_destroy(url);
    return NULL;
}

void url_destroy(url_t *url)
{
    if (!url)
        return;
    free(url->scheme);
    free(url->userinfo);
    free(url->host);
    free(url->port);
    free(url->path);
    free(url->query);
    free(url->fragment);
    free(url);
}

bool url_is_absolute(const url_t *url)
{
    return url && url->scheme != NULL;
}

int url_default_port(const url_t *url)
{
    if (!url || !url->scheme)
        return -1;
    if (strcmp(url->scheme, "http") == 0 || strcmp(url->scheme, "ws") == 0)
        return 80;
    if (strcmp(url->scheme, "https") == 0 || strcmp(url->scheme, "wss") == 0)
        return 443;
    if (strcmp(url->scheme, "ftp") == 0)
        return 21;
    return -1;
}

static char *append(char *dst, const char *src, size_t len)
{
    memcpy(dst, src, len);
    return dst + len;
}

char *url_to_string(const url_t *url)
{
    if (!url)
        return NULL;

    int braced_host = url->host && strchr(url->host, ':') != NULL;

    size_t len = 0;
    if (url->scheme)
        len += strlen(url->scheme) + 1;
    if (url->host)
    {
        len += 2;
        if (url->userinfo)
            len += strlen(url->userinfo) + 1;
        len += strlen(url->host);
        if (braced_host)
            len += 2;
        if (url->port)
            len += strlen(url->port) + 1;
    }
    if (url->path)
        len += strlen(url->path);
    if (url->query)
        len += strlen(url->query) + 1;
    if (url->fragment)
        len += strlen(url->fragment) + 1;

    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    char *w = out;

    if (url->scheme)
    {
        w = append(w, url->scheme, strlen(url->scheme));
        *w++ = ':';
    }
    if (url->host)
    {
        *w++ = '/';
        *w++ = '/';
        if (url->userinfo)
        {
            w = append(w, url->userinfo, strlen(url->userinfo));
            *w++ = '@';
        }
        if (braced_host)
            *w++ = '[';
        w = append(w, url->host, strlen(url->host));
        if (braced_host)
            *w++ = ']';
        if (url->port)
        {
            *w++ = ':';
            w = append(w, url->port, strlen(url->port));
        }
    }
    if (url->path)
        w = append(w, url->path, strlen(url->path));
    if (url->query)
    {
        *w++ = '?';
        w = append(w, url->query, strlen(url->query));
    }
    if (url->fragment)
    {
        *w++ = '#';
        w = append(w, url->fragment, strlen(url->fragment));
    }
    *w = '\0';
    return out;
}

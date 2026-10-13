#include "http/request.h"

#include "message_internal.h"

#include <stdlib.h>
#include <string.h>

request_t *create_request(void)
{
    request_t *request = calloc(1, sizeof(*request));
    if (!request)
        return NULL;

    request->method = msg_strdup("GET");
    request->url = create_url("/");
    request->version = msg_strdup("HTTP/1.1");
    request->headers = create_headers();
    if (!request->method || !request->url || !request->version || !request->headers)
    {
        destroy_request(request);
        return NULL;
    }
    return request;
}

void destroy_request(request_t *request)
{
    if (!request)
        return;
    free(request->method);
    destroy_url(request->url);
    free(request->version);
    destroy_headers(request->headers);
    free(request->body);
    free(request);
}

const char *request_get_method(const request_t *request)
{
    return request ? request->method : NULL;
}

const url_t *request_get_url(const request_t *request)
{
    return request ? request->url : NULL;
}

const char *request_get_version(const request_t *request)
{
    return request ? request->version : NULL;
}

headers_t *request_get_headers(request_t *request)
{
    return request ? request->headers : NULL;
}

const char *request_get_body(const request_t *request, size_t *len)
{
    if (len)
        *len = request ? request->body_len : 0;
    return request ? request->body : NULL;
}

int request_set_method(request_t *request, const char *method)
{
    if (!request || !method || !msg_is_token(method, strlen(method)))
        return -1;
    char *copy = msg_strdup(method);
    if (!copy)
        return -1;
    free(request->method);
    request->method = copy;
    return 0;
}

int request_set_url(request_t *request, const char *target)
{
    if (!request || !target)
        return -1;
    url_t *url = create_url(target);
    if (!url)
        return -1;
    destroy_url(request->url);
    request->url = url;
    return 0;
}

int request_set_version(request_t *request, const char *version)
{
    if (!request || !version || !msg_is_http_version(version))
        return -1;
    char *copy = msg_strdup(version);
    if (!copy)
        return -1;
    free(request->version);
    request->version = copy;
    return 0;
}

int request_set_body(request_t *request, const char *body, size_t len)
{
    if (!request || (!body && len != 0))
        return -1;
    char *copy = NULL;
    if (len > 0)
    {
        copy = msg_dup_n(body, len);
        if (!copy)
            return -1;
    }
    free(request->body);
    request->body = copy;
    request->body_len = copy ? len : 0;
    return 0;
}

request_t *request_parse(const char *data, size_t len)
{
    if (!data)
        return NULL;
    const char *end = data + len;

    const char *crlf = msg_find_crlf(data, end);
    if (!crlf)
        return NULL;
    const char *line_end = crlf;
    size_t line_len = (size_t)(line_end - data);

    /* request-line = method SP request-target SP HTTP-version */
    const char *sp1 = memchr(data, ' ', line_len);
    if (!sp1)
        return NULL;
    size_t method_len = (size_t)(sp1 - data);
    if (!msg_is_token(data, method_len))
        return NULL;

    const char *target = sp1 + 1;
    const char *sp2 = memchr(target, ' ', (size_t)(line_end - target));
    if (!sp2)
        return NULL;
    size_t target_len = (size_t)(sp2 - target);
    if (target_len == 0)
        return NULL;
    size_t version_len = (size_t)(line_end - (sp2 + 1));
    if (!msg_is_http_version_n(sp2 + 1, version_len))
        return NULL;

    char *target_text = msg_dup_n(target, target_len);
    char *method = msg_dup_n(data, method_len);
    char *version = msg_dup_n(sp2 + 1, version_len);
    url_t *url = target_text ? create_url(target_text) : NULL;
    free(target_text);
    if (!method || !version || !url)
    {
        free(method);
        free(version);
        destroy_url(url);
        return NULL;
    }

    request_t *request = calloc(1, sizeof(*request));
    if (!request)
    {
        free(method);
        free(version);
        destroy_url(url);
        return NULL;
    }
    request->method = method;
    request->url = url;
    request->version = version;
    request->headers = create_headers();
    if (!request->headers)
    {
        destroy_request(request);
        return NULL;
    }

    const char *cursor = crlf + 2;
    if (msg_parse_headers(request->headers, &cursor, end) != 0 ||
        msg_parse_body(request->headers, &cursor, end, &request->body,
                       &request->body_len) != 0)
    {
        destroy_request(request);
        return NULL;
    }
    return request;
}

char *request_to_string(const request_t *request, size_t *len)
{
    if (len)
        *len = 0;
    if (!request)
        return NULL;

    msg_buf_t buf;
    msg_buf_init(&buf);
    int ok = 1;

    if (msg_buf_puts(&buf, request->method ? request->method : "") != 0 ||
        msg_buf_putc(&buf, ' ') != 0)
        ok = 0;

    if (ok)
    {
        char *target = request->url ? url_to_string(request->url) : NULL;
        if (request->url && !target)
            ok = 0;
        else if (msg_buf_puts(&buf, target ? target : "") != 0)
            ok = 0;
        free(target);
    }

    if (ok && (msg_buf_putc(&buf, ' ') != 0 ||
               msg_buf_puts(&buf, request->version ? request->version
                                                   : "HTTP/1.1") != 0 ||
               msg_buf_puts(&buf, "\r\n") != 0))
        ok = 0;

    if (ok && msg_serialize_headers(request->headers, &buf) != 0)
        ok = 0;

    if (ok && msg_buf_puts(&buf, "\r\n") != 0)
        ok = 0;

    if (ok)
    {
        if (msg_has_token(request->headers, "Transfer-Encoding", "chunked"))
        {
            if (msg_serialize_chunked(request->body, request->body_len, &buf) != 0)
                ok = 0;
        }
        else if (request->body_len > 0 &&
                 msg_buf_append(&buf, request->body, request->body_len) != 0)
        {
            ok = 0;
        }
    }

    if (!ok)
    {
        msg_buf_free(&buf);
        return NULL;
    }
    if (!buf.data)
        buf.data = msg_strdup(""); /* empty request still yields "" */
    if (!buf.data)
        return NULL;
    if (len)
        *len = buf.len;
    return buf.data;
}

#include "http/response.h"

#include "message_internal.h"

#include <stdlib.h>
#include <string.h>

response_t *create_response(void)
{
    response_t *response = calloc(1, sizeof(*response));
    if (!response)
        return NULL;

    response->version = msg_strdup("HTTP/1.1");
    response->status = 200;
    response->headers = create_headers();
    if (!response->version || !response->headers)
    {
        destroy_response(response);
        return NULL;
    }
    return response;
}

void destroy_response(response_t *response)
{
    if (!response)
        return;
    free(response->version);
    free(response->reason);
    destroy_headers(response->headers);
    free(response->body);
    free(response);
}

const char *response_get_version(const response_t *response)
{
    return response ? response->version : NULL;
}

int response_get_status(const response_t *response)
{
    return response ? response->status : 0;
}

const char *response_get_reason(const response_t *response)
{
    return response ? response->reason : NULL;
}

headers_t *response_get_headers(response_t *response)
{
    return response ? response->headers : NULL;
}

const char *response_get_body(const response_t *response, size_t *len)
{
    if (len)
        *len = response ? response->body_len : 0;
    return response ? response->body : NULL;
}

int response_set_version(response_t *response, const char *version)
{
    if (!response || !version || !msg_is_http_version(version))
        return -1;
    char *copy = msg_strdup(version);
    if (!copy)
        return -1;
    free(response->version);
    response->version = copy;
    return 0;
}

int response_set_status(response_t *response, int status)
{
    if (!response || status < 100 || status > 999)
        return -1;
    response->status = status;
    return 0;
}

int response_set_reason(response_t *response, const char *reason)
{
    if (!response)
        return -1;
    if (reason)
    {
        for (const char *p = reason; *p; p++)
            if (*p == '\r' || *p == '\n')
                return -1;
    }
    char *copy = reason ? msg_strdup(reason) : NULL;
    if (reason && !copy)
        return -1;
    free(response->reason);
    response->reason = copy;
    return 0;
}

int response_set_body(response_t *response, const char *body, size_t len)
{
    if (!response || (!body && len != 0))
        return -1;
    char *copy = NULL;
    if (len > 0)
    {
        copy = msg_dup_n(body, len);
        if (!copy)
            return -1;
    }
    free(response->body);
    response->body = copy;
    response->body_len = copy ? len : 0;
    return 0;
}

static int append_status(msg_buf_t *buf, int status)
{
    if (status < 100 || status > 999)
        return -1;
    char digits[3];
    digits[0] = (char)('0' + status / 100);
    digits[1] = (char)('0' + (status / 10) % 10);
    digits[2] = (char)('0' + status % 10);
    return msg_buf_append(buf, digits, sizeof(digits));
}

response_t *response_parse(const char *data, size_t len)
{
    if (!data)
        return NULL;
    const char *end = data + len;

    const char *crlf = msg_find_crlf(data, end);
    if (!crlf)
        return NULL;
    const char *line_end = crlf;
    size_t line_len = (size_t)(line_end - data);

    /* status-line = HTTP-version SP status-code SP [ reason-phrase ] */
    const char *sp1 = memchr(data, ' ', line_len);
    if (!sp1)
        return NULL;
    size_t version_len = (size_t)(sp1 - data);
    if (!msg_is_http_version_n(data, version_len))
        return NULL;

    const char *code = sp1 + 1;
    if ((size_t)(line_end - code) < 3 || code[0] < '0' || code[0] > '9' ||
        code[1] < '0' || code[1] > '9' || code[2] < '0' || code[2] > '9')
        return NULL;
    int status = (code[0] - '0') * 100 + (code[1] - '0') * 10 + (code[2] - '0');

    /* An optional reason phrase is preceded by a mandatory SP; an empty phrase
     * is stored as NULL so it round-trips identically to an absent one. */
    const char *reason = NULL;
    size_t reason_len = 0;
    if (code + 3 < line_end)
    {
        if (code[3] != ' ')
            return NULL;
        reason = code + 4;
        reason_len = (size_t)(line_end - reason);
    }

    char *version = msg_dup_n(data, version_len);
    char *reason_copy = reason_len > 0 ? msg_dup_n(reason, reason_len) : NULL;
    if (!version || (reason_len > 0 && !reason_copy))
    {
        free(version);
        free(reason_copy);
        return NULL;
    }

    response_t *response = calloc(1, sizeof(*response));
    if (!response)
    {
        free(version);
        free(reason_copy);
        return NULL;
    }
    response->version = version;
    response->status = status;
    response->reason = reason_copy;
    response->headers = create_headers();
    if (!response->headers)
    {
        destroy_response(response);
        return NULL;
    }

    const char *cursor = crlf + 2;
    if (msg_parse_headers(response->headers, &cursor, end) != 0 ||
        msg_parse_body(response->headers, &cursor, end, &response->body,
                       &response->body_len) != 0)
    {
        destroy_response(response);
        return NULL;
    }
    return response;
}

char *response_to_string(const response_t *response, size_t *len)
{
    if (len)
        *len = 0;
    if (!response)
        return NULL;

    msg_buf_t buf;
    msg_buf_init(&buf);
    int ok = 1;

    if (msg_buf_puts(&buf, response->version ? response->version
                                             : "HTTP/1.1") != 0 ||
        msg_buf_putc(&buf, ' ') != 0 || append_status(&buf, response->status) != 0 ||
        msg_buf_putc(&buf, ' ') != 0)
        ok = 0;

    if (ok && response->reason && msg_buf_puts(&buf, response->reason) != 0)
        ok = 0;

    if (ok && msg_buf_puts(&buf, "\r\n") != 0)
        ok = 0;

    if (ok && msg_serialize_headers(response->headers, &buf) != 0)
        ok = 0;

    if (ok && msg_buf_puts(&buf, "\r\n") != 0)
        ok = 0;

    if (ok)
    {
        if (msg_has_token(response->headers, "Transfer-Encoding", "chunked"))
        {
            if (msg_serialize_chunked(response->body, response->body_len, &buf) != 0)
                ok = 0;
        }
        else if (response->body_len > 0 &&
                 msg_buf_append(&buf, response->body, response->body_len) != 0)
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
        buf.data = msg_strdup("");
    if (!buf.data)
        return NULL;
    if (len)
        *len = buf.len;
    return buf.data;
}

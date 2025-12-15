#include "HTTPRequest.h"
#include "HTTPUtils.h"
#include "common_utils.h"

void HTTPRequest::reset(void)
{
    cur_state = State::receive_header;
    header_terminator = NULL;
    remaining = 0;
    content_length = NULL;
    header_size = 0;
    read_size = 0;
    total_size = 0;
}

HTTPRequest::HTTPRequest(int fd, size_t buffer_size) : fd(fd), buffer_size(buffer_size)
{
    buffer = new char[buffer_size + 1]{};
    reset();
}

HTTPRequest::~HTTPRequest()
{
    delete[] buffer;
}

HTTPRequest::HTTPRequest(HTTPRequest &&r)
{
    fd = r.fd;
    buffer = r.buffer;
    buffer_size = r.buffer_size;
    cur_state = r.cur_state;
    header_terminator = r.header_terminator;
    remaining = r.remaining;
    content_length = r.content_length;
    header_size = r.header_size;
    read_size = r.read_size;
    total_size = r.total_size;

    r.fd = -1;
    r.buffer = NULL;
    r.buffer_size = 0;
}

HTTPRequest::Method HTTPRequest::get_method()
{
    if (!startcmp(buffer, "GET"))
        return HTTPRequest::Method::GET;
    if (!startcmp(buffer, "HEAD"))
        return HTTPRequest::Method::HEAD;
    if (!startcmp(buffer, "POST"))
        return HTTPRequest::Method::POST;
    if (!startcmp(buffer, "PUT"))
        return HTTPRequest::Method::PUT;
    if (!startcmp(buffer, "DELETE"))
        return HTTPRequest::Method::DELETE;
    if (!startcmp(buffer, "OPTIONS"))
        return HTTPRequest::Method::OPTIONS;
    if (!startcmp(buffer, "PATCH"))
        return HTTPRequest::Method::PATCH;
    return HTTPRequest::Method::nil;
}

// int fd, char *buffer, int max, int &header_size
HTTPRequest::ProcessStatus HTTPRequest::receive_header()
{
    static int nr = 0;

    int n = recv(fd, buffer + nr, buffer_size - nr, 0);
    if (n <= 0)
        return ProcessStatus::error;
    buffer[read_size + n] = 0;
    if (read_size == 0)
    {
        // TODO: implement regex-style check for valid request line
        // this would allow requests where first line is split across multiple recv calls
        if (get_method() == Method::nil)
            return ProcessStatus::error;
        char *http = strstr(buffer, "HTTP/1.1");
        if (!http)
            return ProcessStatus::error;
        char *first_crlf = strstr(buffer, CRLF);
        if (!first_crlf || first_crlf - http < 0)
            return ProcessStatus::error;
        char *url = strchr(buffer, '/');
        if (!url || url - http > 0)
            return ProcessStatus::error;
    }
    read_size += n;

    if (!(header_terminator = strstr(buffer, CRLFx2)))
    {
        if (read_size >= buffer_size)
            return ProcessStatus::error;
        return ProcessStatus::incomplete;
    }

    header_size = header_terminator - buffer + strlen(CRLFx2);
    return ProcessStatus::complete;
}

HTTPRequest::ProcessStatus HTTPRequest::process_header()
{
    if (!content_length)
    {
        content_length = strstr(buffer, "Content-Length: ");

        if (!content_length)
        {
            total_size = header_size;
            buffer[read_size] = 0;
            return ProcessStatus::complete;
        }

        content_length += strlen("Content-Length: ");
        int digits = 0;
        while (content_length[digits] >= '0' && content_length[digits] <= '9')
            digits++;
        content_length[digits] = 0;
        uint64_t length = strtoull(content_length, NULL, 0);
        content_length[digits] = '\r';
        uint64_t content_read = read_size - header_size;
        remaining = length - content_read;
        total_size = remaining + read_size;
    }

    if (!remaining || buffer_size - read_size == 0)
    {
        buffer[read_size] = 0;
        return ProcessStatus::complete;
    }

    int n = recv(fd, buffer + read_size, total_size > buffer_size ? buffer_size - read_size : remaining, 0);
    if (n <= 0)
        return ProcessStatus::error;
    read_size += n;
    remaining -= n;

    if (remaining && buffer_size - read_size > 0)
        return ProcessStatus::incomplete;

    buffer[read_size] = 0;
    return ProcessStatus::complete;
}

HTTPRequest::ProcessStatus HTTPRequest::process(void)
{
    HTTPRequest::ProcessStatus last_status;

    do
    {
        switch (cur_state)
        {
        case State::receive_header:
            last_status = receive_header();
            if (last_status != ProcessStatus::complete)
                return ProcessStatus::error;

            cur_state = State::process_header;
            break;

        case State::process_header:
            last_status = process_header();
            if (last_status != ProcessStatus::complete)
                return last_status;

            cur_state = State::done;
            break;
        case State::done:
            return ProcessStatus::error;
        }
    } while (cur_state != State::done);

    return ProcessStatus::complete;
}

char *HTTPRequest::get_data()
{
    return buffer;
}

int HTTPRequest::get_fd()
{
    return fd;
}

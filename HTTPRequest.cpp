#include "HTTPRequest.h"
#include "HTTPUtils.h"
#include "common_utils.h"
#include <boost/filesystem.hpp>

#define MAX_ADDR_LEN 108

namespace fs = boost::filesystem;

void HTTPRequest::reset(void)
{
    cur_state = State::receive_proxyv2_header;
    header_terminator = NULL;
    remaining = 0;
    content_length = NULL;
    header_size = 0;
    read_size = 0;
    total_size = 0;
    if (recv_file_data)
    {
        delete recv_file_data;
        recv_file_data = NULL;
    }
    new_request = true;
    proxy_processed = false;
    memset(&proxyv2_header, 0, sizeof(proxyv2_header));
}

HTTPRequest::HTTPRequest(int fd, size_t buffer_size, std::string client_addr) : fd(fd), buffer_size(buffer_size), client_addr(client_addr)
{
    buffer = new char[buffer_size + 1]{};
    reset();
}

HTTPRequest::~HTTPRequest()
{
    if (buffer)
        delete[] buffer;
    if (recv_file_data)
        delete recv_file_data;
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
    recv_file_data = r.recv_file_data;
    proxyv2_header = r.proxyv2_header;
    proxyv2_address = r.proxyv2_address;
    proxy_processed = r.proxy_processed;
    new_request = r.new_request;

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

const uint8_t proxy_signature[12] = {0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A};

HTTPRequest::ProcessStatus HTTPRequest::receive_proxyv2_data()
{
    if (new_request) // Nothing received yet so receive header
    {
        int n = recv(fd, &proxyv2_header, sizeof(proxyv2_header), 0);
        if (n != sizeof(proxyv2_header)) // RFC specifies sender must send all at once
            return ProcessStatus::error;

        if (memcmp(&proxyv2_header, proxy_signature, sizeof(proxy_signature))) // check protocol signature;
        {
            // Relay to HTTP processing as no proxy header present
            memcpy(buffer, &proxyv2_header, n);
            read_size += n;
            proxy_processed = true;
            return ProcessStatus::complete;
        }
        new_request = false;
        return ProcessStatus::incomplete;
    }
    // Header received so receive proxyv2 address info
    uint16_t addr_len = ntohs(proxyv2_header.len);
    int n = recv(fd, &proxyv2_address, addr_len, 0);
    if (n != addr_len) // RFC doesn't specify all at once (?) but require it anyway
        return ProcessStatus::error;

    // Set client_addr based on received info
    char addr_buffer[MAX_ADDR_LEN + 1];
    switch (proxyv2_header.fam)
    {
    case static_cast<uint8_t>(ProxyFamily::tcp_ipv4):
        client_addr = inet_ntop(AF_INET, &proxyv2_address.ipv4_addr.src_addr, addr_buffer, MAX_ADDR_LEN);
        break;
    case static_cast<uint8_t>(ProxyFamily::tcp_ipv6):
        client_addr = inet_ntop(AF_INET6, proxyv2_address.ipv6_addr.src_addr, addr_buffer, MAX_ADDR_LEN);
        break;
    case static_cast<uint8_t>(ProxyFamily::unix_stream):
        client_addr = strncpy(addr_buffer, (char *)proxyv2_address.unix_addr.src_addr, MAX_ADDR_LEN);
        break;
    default:
        break;
    }

    new_request = true;
    proxy_processed = true;
    return ProcessStatus::complete;
}

std::string HTTPRequest::get_client_addr()
{
    return client_addr;
}

HTTPRequest::ProcessStatus HTTPRequest::receive_header()
{
    new_request = false;
    int n = recv(fd, buffer + read_size, buffer_size - read_size, 0);
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
        case State::receive_proxyv2_header:
            // Currently not implemented, move to next state
            last_status = receive_proxyv2_data();
            if (last_status != ProcessStatus::complete)
                return last_status;

            cur_state = State::receive_header;
            // for now the proxy read function reads strictly the header so we need to return
            return ProcessStatus::incomplete;
            break;
        case State::receive_header:
            last_status = receive_header();
            if (last_status != ProcessStatus::complete)
                return last_status;

            cur_state = State::process_header;
            break;

        case State::process_header:
            last_status = process_header();
            if (last_status != ProcessStatus::complete)
                return last_status;

            cur_state = State::done;
            break;
        case State::receive_file:
            last_status = receive_file_data();
            if (last_status != ProcessStatus::complete)
                return last_status;

            cur_state = State::done;
            break;
        case State::done:
            // Try to process/receive after done which is not allowed
            return ProcessStatus::error;
        }
    } while (cur_state != State::done);

    return ProcessStatus::complete;
}

HTTPRequest::ProcessStatus HTTPRequest::handle_receive_file_writes()
{
    auto &f = *recv_file_data;
    // TODO: build mechanism to check cancel of receive and also timeout
    f.file->write(buffer, read_size);
    if (!remaining)
    {
        uint64_t size = f.file->tellp(), bs = f.boundary.size();
        f.file->close();
        size_t offset = bs + strlen("--") + strlen(CRLF);
        if (strncmp(buffer + read_size - offset, f.boundary.c_str(), bs))
        {
            fs::remove_all(f.filename);
            return ProcessStatus::error;
        }
        fs::resize_file(f.filename, size - offset - strlen("--") - strlen(CRLF));

        return ProcessStatus::complete;
    }
    read_size = 0;
    return ProcessStatus::incomplete;
}

HTTPRequest::ProcessStatus HTTPRequest::receive_file_data()
{
    if (!recv_file_data)
        return ProcessStatus::error;

    uint64_t buffer_remaining = buffer_size - read_size;
    int n = recv(fd, buffer + read_size, remaining > buffer_remaining ? buffer_remaining : remaining, 0);
    if (n <= 0)
    {
        fs::remove_all(recv_file_data->filename);
        return ProcessStatus::error;
    }

    read_size += n;
    remaining -= n;

    if (read_size == buffer_size || remaining == 0)
        return handle_receive_file_writes();
    return ProcessStatus::incomplete;
}

char *HTTPRequest::get_data()
{
    return buffer;
}

int HTTPRequest::get_fd()
{
    return fd;
}

bool HTTPRequest::is_new()
{
    return new_request && proxy_processed;
}

HTTPRequest::ProcessStatus HTTPRequest::start_file_reception(std::string filename, std::string boundary, char *read_data, size_t read_size, uint64_t remaining)
{
    cur_state = State::receive_file;
    this->remaining = remaining;
    this->read_size = read_size;

    char tmp[buffer_size];
    memcpy(tmp, read_data, read_size);
    memcpy(buffer, tmp, read_size);

    recv_file_data = new file_receive_data(filename, boundary);
    if (!remaining)
        return handle_receive_file_writes();
    return ProcessStatus::incomplete;
}

HTTPRequest::file_receive_data::file_receive_data(std::string filename, std::string boundary)
    : filename(filename), boundary(boundary)
{
    file = new std::ofstream(filename, std::ios::binary);
}

HTTPRequest::file_receive_data::file_receive_data(file_receive_data &&f)
{
    this->file = f.file;
    f.file = NULL;
    this->filename = f.filename;
    this->boundary = f.boundary;
    f.filename = "";
}

HTTPRequest::file_receive_data::~file_receive_data()
{
    if (file)
        delete file;
}

#include "HTTPresponse.h"
#include <iostream>
#include <string.h>
#include <sstream>

HTTPresponse::HTTPresponse(int code)
{
    if (code == PHONY)
        return;

    response += "HTTP/1.1 ";
    response += to_string(code) + " ";

    switch (code)
    {
    case 200:
        response += "OK";
        break;
    case 206:
        response += "Partial Content";
        break;
    case 302:
        response += "Found";
        break;
    case 303:
        response += "See Other";
        break;
    case 400:
        response += "Bad Request";
        break;
    case 401:
        response += "Unauthorized";
        break;
    case 404:
        response += "Not Found";
        break;
    case 422:
        response += "Unprocessable Entity";
        break;
    case 501:
        response += "Not Implemented";
        break;
    case 503:
        response += "Service Unavailable";
        break;
    default:
        break;
    }

    response += CRLF;
}

HTTPresponse &HTTPresponse::end_header()
{
    response += CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::content_length(uint64_t length)
{
    response += "Content-Length: ";
    response += to_string(length);
    response += CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::content_type(MIME type)
{
    response += "Content-Type: ";
    switch (type)
    {
    case MIME::html:
        response += "text/html";
        break;
    case MIME::javascript:
        response += "text/javascript";
        break;
    case MIME::css:
        response += "text/css";
        break;
    case MIME::octet_stream:
        response += "application/octet-stream";
        break;
    case MIME::text:
        response += "text/plain";
        break;
    case MIME::png:
        response += "image/png";
        break;
    case MIME::jpeg:
        response += "image/jpeg";
        break;
    case MIME::icon:
        response += "image/x-icon";
        break;
    case MIME::zip:
        response += "application/zip";
        break;
    case MIME::mp4:
        response += "video/mp4";
        break;
    case MIME::mkv:
        response += "video/x-matroska";
        break;
    case MIME::pdf:
        response += "application/pdf";
        break;
    default:
        break;
    }
    response += CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::content_disposition(DISP disposition, string filename)
{
    response += "Content-Disposition: ";
    switch (disposition)
    {
    case DISP::Attachment:
        response += "attachment; filename=\"" + filename + "\"";
        break;
    case DISP::Inline:
        response += "inline";
        break;
    default:
        break;
    }
    response += CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::location(string url)
{
    response += "Location: " + url + CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::cookie(Cookie *cookie)
{
    if (!cookie)
        return *this;
    response += "Set-Cookie: " + cookie->identifier() + "=" + cookie->val() + "; " + cookie->expiry() + "; Path=/" CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::data(const char *filename)
{
    std::ifstream f(filename, std::ios::binary);
    response += std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return *this;
}

HTTPresponse &HTTPresponse::file_promise(const char *filename)
{
    filename_str = filename;
    is_promise = true;
    return *this;
}

HTTPresponse &HTTPresponse::file_attachment(string data, MIME type)
{
    content_type(type).content_length(data.length()).end_header();
    response += data;
    return *this;
}

HTTPresponse &HTTPresponse::file_attachment(const char *filename, MIME type, uint64_t begin_offset)
{
    this->filename_str = filename;
    this->begin_offset = begin_offset;
    attach_file(type);
    return *this;
}

HTTPresponse &HTTPresponse::file_attachment(const char *filename, MIME type, std::function<void(const char *, void *)> action, void *action_object)
{
    file_action = action;
    this->action_object = action_object;
    file_attachment(filename, type);
    return *this;
}

HTTPresponse &HTTPresponse::attach_file(MIME type)
{
    is_promise = false;
    std::ifstream file(filename_str, std::ios::binary);
    if (!chunked)
    {
        auto beg = file.tellg();
        uint64_t total = file.seekg(0, std::ios::end).tellg() - beg - begin_offset;
        content_type(type).content_length(total).end_header();
        return *this;
    }
    content_type(type).end_header();
    return *this;
}

HTTPresponse &HTTPresponse::attach_file(MIME type, std::function<void(const char *, void *)> action, void *action_object)
{
    file_action = action;
    this->action_object = action_object;
    attach_file(type);
    return *this;
}

HTTPresponse &HTTPresponse::access_control(string control)
{
    response += "Access-Control-Allow-Origin: " + control + CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::content_range(uint64_t begin_offset)
{
    this->begin_offset = begin_offset;
    std::ifstream file(filename_str, std::ios::binary);
    auto beg = file.tellg();
    uint64_t total = file.seekg(0, std::ios::end).tellg() - beg;
    response += "Content-Range: bytes " + to_string(begin_offset) + "-" + to_string(total - 1) + "/" + to_string(total) + CRLF;
    return *this;
}

HTTPresponse &HTTPresponse::transfer_encoding(ENCODING encoding)
{
    if (encoding == ENCODING::none)
        return *this;
    this->encoding = encoding;
    response += "Transfer-Encoding: chunked" CRLF;
    response += "Content-Encoding: ";
    switch (encoding)
    {
    case ENCODING::deflate:
        response += "deflate";
        chunked = true;
        break;
    case ENCODING::gzip:
        response += "gzip";
        chunked = true;
        break;

    default:
        break;
    }
    response += CRLF;
    return *this;
}

HTTPresponse::filesegment_iterator::filesegment_iterator(HTTPresponse *parent, size_t read_fragment_size, size_t max_fragment_size, uint64_t begin_offset)
{
    this->read_fragment_size = read_fragment_size;
    this->max_fragment_size = max_fragment_size;
    file = new std::ifstream(parent->filename_str, std::ios::binary);
    filename = parent->filename_str;
    action = parent->file_action;
    action_object = parent->action_object;
    auto beg = file->tellg();
    remaining = file->seekg(0, std::ios::end).tellg() - beg;
    file->seekg(begin_offset, std::ios::beg);
    file_data.fragment = new char[max_fragment_size];
    (*this)++;
}

HTTPresponse::filesegment_iterator::filesegment_iterator(HTTPresponse *parent, size_t read_fragment_size, uint64_t begin_offset)
    : filesegment_iterator(parent, read_fragment_size, read_fragment_size, begin_offset)
{
}

HTTPresponse::filesegment_iterator::filesegment_iterator(HTTPresponse::filesegment_iterator &&f)
{
    this->file = f.file;
    this->action = f.action;
    this->action_object = f.action_object;
    f.file = NULL;
    this->filename = f.filename;
    this->file_data = f.file_data;
    f.file_data.fragment = NULL;
    f.file_data.size = 0;
    this->max_fragment_size = f.max_fragment_size;
    this->read_fragment_size = f.read_fragment_size;
    this->remaining = f.remaining;
}

HTTPresponse::filesegment_iterator::~filesegment_iterator()
{
    if (file)
    {
        delete file;
        delete[] file_data.fragment;
        if (action)
            action(filename.c_str(), action_object);
    }
}

HTTPresponse::filesegment_iterator &HTTPresponse::filesegment_iterator::operator++(int)
{
    if (!remaining)
    {
        file_data.size = 0;
        return *this;
    }
    file->read(file_data.fragment, read_fragment_size);
    file_data.size = file->gcount();
    remaining -= file_data.size;
    file_data.cur_frag_sent = 0;
    return *this;
}

HTTPresponse::filesegment_iterator::data *HTTPresponse::filesegment_iterator::operator->()
{
    return &file_data;
}

HTTPresponse::filesegment_iterator *HTTPresponse::begin_file_transfer(size_t fragment_size)
{
    switch (encoding)
    {
    case ENCODING::gzip:
    case ENCODING::deflate:
        return new zlib_compress_iterator(this, fragment_size, begin_offset, encoding);
    default:
        return new filesegment_iterator(this, fragment_size, begin_offset);
    }
}

size_t HTTPresponse::zlib_compress_iterator::init_deflate(size_t read_fragment_size, HTTPresponse::ENCODING encoding, int max_number_of_hex_digits)
{
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    this->max_number_of_hex_digits = max_number_of_hex_digits;

    // This deflateInit2 call uses the same default parameters as deflateInit, the only change is choosing the encoding by setting bit 5 in the windowBits parameter
    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | (encoding == HTTPresponse::ENCODING::gzip ? 16 : 0), 8, Z_DEFAULT_STRATEGY);

    max_compressed_size = deflateBound(&stream, read_fragment_size);
    // Fragment buffer size chosen to accommodate the maximum size of the deflate output + bytes for the hex size sent by http
    // + 2 bytes for CRLF + 5 bytes for "0 CRLF CRLF" sent at the end
    max_compressed_fragment_size = max_compressed_size + max_number_of_hex_digits + 2 + 5;
    return max_compressed_fragment_size;
}

void HTTPresponse::zlib_compress_iterator::deflate_chunk_fragment()
{
    stream.avail_in = file_data.size;
    stream.next_in = (Bytef *)file_data.fragment;
    stream.avail_out = max_compressed_size;
    stream.next_out = (Bytef *)(compressed + max_number_of_hex_digits);

    // TODO: try to find a way to do this with Z_NO_FLUSH instead of Z_SYNC_FLUSH. Or maybe try Z_PARTIAL_FLUSH as well
    deflate(&stream, remaining ? Z_SYNC_FLUSH : Z_FINISH);

    // TODO: test if reserving space in the buffer for number of bytes instead of copying the buffer is faster
    size_t compressed_size = max_compressed_size - stream.avail_out;
    std::stringstream hex_stream;
    hex_stream << std::hex << compressed_size;
    string compressed_size_hex_str = hex_stream.str() + CRLF;
    size_t total_size = 0;
    // Get where the fragment actually begins in the buffer
    int fragment_pointer_offset = max_number_of_hex_digits - compressed_size_hex_str.length();

    memcpy(compressed + fragment_pointer_offset, compressed_size_hex_str.c_str(), compressed_size_hex_str.length());
    total_size += compressed_size_hex_str.length();
    // memcpy(file_data.fragment + total_size, compressed, compressed_size);
    total_size += compressed_size;
    memcpy(compressed + fragment_pointer_offset + total_size, CRLF, strlen(CRLF));
    total_size += strlen(CRLF);
    if (!remaining)
    {
        memcpy(compressed + fragment_pointer_offset + total_size, "0" CRLF CRLF, strlen("0" CRLFx2));
        total_size += strlen("0" CRLFx2);
    }
    file_data.size = total_size;

    // Subtract the last offset from the fragment that contained the incoming data to get the actual fragment
    char *actual_in_fragment = file_data.fragment - last_fragment_pointer_offset;
    // Set new fragment to the compressed data
    file_data.fragment = compressed + fragment_pointer_offset;
    // Update the offset
    last_fragment_pointer_offset = fragment_pointer_offset;
    // Set compressed to old fragment
    compressed = actual_in_fragment;
}

HTTPresponse::zlib_compress_iterator::zlib_compress_iterator(HTTPresponse *parent, size_t read_fragment_size, uint64_t begin_offset, HTTPresponse::ENCODING encoding)
    : HTTPresponse::filesegment_iterator(parent, read_fragment_size, init_deflate(read_fragment_size, encoding, 2 * sizeof(size_t)), begin_offset)
{
    // Ties in with the in-class initialization of compressed. If we move this to init_deflate, compressed should
    // *not* be initialized to NULL in the class definition anymore or else it gets overwritten
    // Same for max_number_of_hex_digits, in-class initialization is undefined when variable is used in
    // initializer list
    compressed = new char[max_compressed_fragment_size];
    // Initially, the data doesn't come offset
    last_fragment_pointer_offset = 0;

    deflate_chunk_fragment();
}

HTTPresponse::zlib_compress_iterator::zlib_compress_iterator(HTTPresponse::zlib_compress_iterator &&f)
    : HTTPresponse::filesegment_iterator(std::move(f))
{
    this->stream = f.stream;
    this->compressed = f.compressed;
    f.compressed = NULL;
}

HTTPresponse::zlib_compress_iterator::~zlib_compress_iterator()
{
    if (file)
    {
        // Undo last offset on fragment
        file_data.fragment -= last_fragment_pointer_offset;
        delete[] compressed;
        deflateEnd(&stream);
    }
}

HTTPresponse::zlib_compress_iterator &HTTPresponse::zlib_compress_iterator::operator++(int)
{
    filesegment_iterator::operator++(1);
    if (!file_data.size)
        return *this;

    deflate_chunk_fragment();
    return *this;
}

string HTTPresponse::get_filename()
{
    return filename_str;
}

bool HTTPresponse::filesegment_iterator::has_next()
{
    return file_data.size != 0;
}

bool HTTPresponse::is_multifragment_transfer()
{
    return filename_str != "";
}

bool HTTPresponse::is_promise_transfer()
{
    return is_promise;
}

bool HTTPresponse::is_phony()
{
    return response == "";
}

const char *HTTPresponse::to_c_str()
{
    return response.c_str();
}

std::size_t HTTPresponse::size()
{
    return response.size();
}

ostream &operator<<(ostream &out, const HTTPresponse &r)
{
    out << r.response;
    return out;
}
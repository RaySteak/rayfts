#include "HTTPresponse.h"
#include <iostream>

HTTPresponse::HTTPresponse(int code)
{
    response += "HTTP/1.1 ";
    response += to_string(code) + " ";

    switch (code)
    {
    case 200:
        response += "OK";
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
    case 501:
        response += "Not Implemented";
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

HTTPresponse &HTTPresponse::content_length(unsigned int length)
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
    case MIME::octet_stream:
        response += "application/octet-stream";
        break;
    case MIME::text:
        response += "text/plain";
        break;
    case MIME::png:
        response += "image/png";
        break;
    case MIME::icon:
        response += "image/x-icon";
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

HTTPresponse &HTTPresponse::data(const char *filename)
{
    std::ifstream file(filename, std::ios::binary);
    response += std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return *this;
}

HTTPresponse &HTTPresponse::file_attachment(string data, MIME type)
{
    content_type(type).content_length(data.length()).end_header();
    response += data;
    return *this;
}

HTTPresponse &HTTPresponse::file_attachment(const char *filename, MIME type)
{
    file = new std::ifstream(filename, std::ios::binary);
    fragment = new char[max_fragment_size];
    auto beg = file->tellg();
    remaining = file->seekg(0, std::ios::end).tellg() - beg;
    file->seekg(0, std::ios::beg);
    content_type(type).content_length(remaining).end_header();
    return *this;
}

HTTPresponse &HTTPresponse::next_file_segment()
{
    if (!remaining)
    {
        delete file;
        file = NULL;
        delete fragment;
        fragment = NULL;
        return *this;
    }
    file->read(fragment, max_fragment_size);
    last_read = file->gcount();
    remaining -= last_read;
    //std::cout << "Am mai citit inca " << last_read << "si mai avem " << remaining << '\n';
    return *this;
}

void HTTPresponse::begin_file_transfer()
{
    next_file_segment();
}

bool HTTPresponse::has_more_segments()
{
    return file != NULL;
}

size_t HTTPresponse::segment_size()
{
    return last_read;
}

char *HTTPresponse::get_file_segment()
{
    return fragment;
}

HTTPresponse &HTTPresponse::cookie(Cookie *cookie)
{
    if (!cookie)
        return *this;
    response += "Set-Cookie: " + cookie->identifier() + "=" + cookie->val() + ";" + cookie->expiry() + CRLF;
    return *this;
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
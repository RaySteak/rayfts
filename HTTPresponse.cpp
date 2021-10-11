#include "HTTPresponse.h"
#include <iostream>

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

HTTPresponse &HTTPresponse::file_attachment(const char *filename, MIME type)
{
    this->filename_str = filename;
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
    auto beg = file.tellg();
    uint64_t total = file.seekg(0, std::ios::end).tellg() - beg;
    content_type(type).content_length(total).end_header();
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

HTTPresponse::filesegment_iterator::filesegment_iterator(HTTPresponse *parent, size_t max_fragment_size)
{
    this->max_fragment_size = send_fragment_size = max_fragment_size;
    file = new std::ifstream(parent->filename_str, std::ios::binary);
    filename = parent->filename_str;
    action = parent->file_action;
    action_object = parent->action_object;
    auto beg = file->tellg();
    remaining = file->seekg(0, std::ios::end).tellg() - beg;
    file->seekg(0, std::ios::beg);
    file_data.fragment = new char[max_fragment_size];
    (*this)++;
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
    this->send_fragment_size = f.send_fragment_size;
    this->remaining = f.remaining;
}

HTTPresponse::filesegment_iterator::~filesegment_iterator()
{
    if (file)
    {
        delete file;
        delete file_data.fragment;
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
    file->read(file_data.fragment, send_fragment_size);
    file_data.size = file->gcount();
    remaining -= file_data.size;
    return *this;
}

HTTPresponse::filesegment_iterator::data *HTTPresponse::filesegment_iterator::operator->()
{
    return &file_data;
}

HTTPresponse::filesegment_iterator HTTPresponse::begin_file_transfer(size_t fragment_size)
{
    return filesegment_iterator(this, fragment_size);
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
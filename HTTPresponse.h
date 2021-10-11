#pragma once

#include <string>
#include <ostream>
#include <fstream>
#include <functional>
#include "Cookie.h"

#define CRLF "\r\n"
#define CRLFx2 "\r\n\r\n"

using std::ostream;
using std::string;
using std::to_string;

class HTTPresponse
{
private:
    string response, filename_str = "";
    uint64_t begin_offset = 0;
    std::function<void(const char *, void *)> file_action;
    bool is_promise = false;
    void *action_object = NULL;

public:
    static const int PHONY = 0; // means response must be sent later

    enum class MIME
    {
        octet_stream,
        html,
        text,
        png,
        javascript,
        css,
        icon,
        zip,
        mp4,
        mkv
    };

    enum class DISP
    {
        Inline,
        Attachment
    };

    HTTPresponse(int code);
    HTTPresponse &end_header();
    HTTPresponse &content_length(uint64_t length);
    HTTPresponse &content_type(MIME type);
    HTTPresponse &content_disposition(DISP disposition, string filename = "");
    HTTPresponse &location(string url);
    HTTPresponse &data(const char *filename);         //use with care, adds whole data from file on heap
    HTTPresponse &file_promise(const char *filename); //only sets the filename string inside the class
    HTTPresponse &file_attachment(string data, MIME type);
    HTTPresponse &file_attachment(const char *filename, MIME type, uint64_t begin_offset = 0);
    // use for when you want a certain action executed when finishing file transfer, e.g. removing temporary file when done
    HTTPresponse &file_attachment(const char *filename, MIME type, std::function<void(const char *, void *)> action, void *action_object);
    HTTPresponse &attach_file(MIME type);
    HTTPresponse &attach_file(MIME type, std::function<void(const char *, void *)> action, void *action_object);
    HTTPresponse &access_control(string control);
    HTTPresponse &content_range(uint64_t begin_offset); //only support begin offsets for now
    class filesegment_iterator
    {
    private:
        struct data
        {
            size_t size = 0;
            char *fragment = NULL;
        } file_data;
        size_t max_fragment_size, send_fragment_size;
        uint64_t remaining = 0;
        string filename;
        std::ifstream *file = NULL;
        std::function<void(const char *, void *)> action;
        void *action_object;

    public:
        filesegment_iterator(HTTPresponse *parent, size_t max_fragment_size, uint64_t begin_offset);
        filesegment_iterator(filesegment_iterator &&f);
        filesegment_iterator(const filesegment_iterator &f) = default;
        ~filesegment_iterator();
        bool has_next();
        filesegment_iterator &operator++(int);
        data *operator->();
    };
    filesegment_iterator begin_file_transfer(size_t fragment_size = 8 * (1 << 10) /* 8KB */);
    string get_filename();
    bool is_multifragment_transfer();
    bool is_promise_transfer();
    bool is_phony();
    HTTPresponse &cookie(Cookie *cookie);
    const char *to_c_str();
    std::size_t size();
    friend ostream &operator<<(ostream &out, const HTTPresponse &r);
};
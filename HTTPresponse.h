#pragma once
#include <string>
#include <ostream>
#include <fstream>
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

public:
    static const int PHONY = 0; // means response must be sent later

    enum class MIME
    {
        octet_stream,
        html,
        text,
        png,
        javascript,
        icon
    };

    HTTPresponse(int code);
    HTTPresponse &end_header();
    HTTPresponse &content_length(unsigned int length);
    HTTPresponse &content_type(MIME type);
    HTTPresponse &location(string url);
    HTTPresponse &data(const char *filename); //use with care, adds whole data from file on heap
    HTTPresponse &file_attachment(string data, MIME type);
    HTTPresponse &file_attachment(const char *filename, MIME type);
    class filesegment_iterator
    {
    private:
        struct data
        {
            size_t size = 0;
            char *fragment = NULL;
        } file_data;
        size_t max_fragment_size, send_fragment_size;
        size_t remaining = 0;
        std::ifstream *file = NULL;
        HTTPresponse *parent;

    public:
        filesegment_iterator(HTTPresponse *parent, size_t max_fragment_size);
        filesegment_iterator(filesegment_iterator &&f);
        ~filesegment_iterator();
        bool has_next();
        //void update_fragment_size(uint64_t waited_nanosecs);
        filesegment_iterator &operator++(int);
        data *operator->();
    };
    filesegment_iterator begin_file_transfer(size_t fragment_size = 8 * (1 << 10) /* 8KB */);
    bool is_multifragment_transfer();
    bool is_phony();
    HTTPresponse &cookie(Cookie *cookie);
    const char *to_c_str();
    std::size_t size();
    friend ostream &operator<<(ostream &out, const HTTPresponse &r);
};
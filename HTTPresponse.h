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
    const int max_fragment_size = 8 * (1 << 10);
    char *fragment = NULL;
    size_t last_read = 0, remaining = 0;
    string response, filename_str;
    std::ifstream *file = NULL;

public:
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
    HTTPresponse &next_file_segment();
    void begin_file_transfer();
    bool has_more_segments();
    size_t segment_size();
    char *get_file_segment();
    HTTPresponse &cookie(Cookie *cookie);
    const char *to_c_str();
    std::size_t size();
    friend ostream &operator<<(ostream &out, const HTTPresponse &r);
};
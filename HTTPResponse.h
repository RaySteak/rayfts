#pragma once

#include <string>
#include <ostream>
#include <fstream>
#include <functional>
#include <zlib.h>
#include "Cookie.h"

using std::ostream;
using std::string;
using std::to_string;

class HTTPResponse
{
public:
    enum class MIME
    {
        octet_stream,
        html,
        text,
        png,
        jpeg,
        svg,
        javascript,
        css,
        icon,
        zip,
        mp4,
        mkv,
        pdf
    };

    enum class DISP
    {
        Inline,
        Attachment
    };

    enum class ENCODING
    {
        deflate,
        gzip,
        none
    };

private:
    string response, filename_str = "";
    uint64_t begin_offset = 0;
    std::function<void(const char *, void *)> file_action;
    bool is_promise = false;
    void *action_object = NULL;
    ENCODING encoding = ENCODING::none;
    bool chunked = false;

public:
    static const int PHONY = 0; // means response must be sent later

    HTTPResponse(int code);
    HTTPResponse &end_header();
    HTTPResponse &content_length(uint64_t length);
    HTTPResponse &content_type(MIME type);
    HTTPResponse &content_disposition(DISP disposition, string filename = "");
    HTTPResponse &location(string url);
    HTTPResponse &cookie(Cookie *cookie);
    HTTPResponse &data(const char *filename);         // use with care, adds whole data from file on heap
    HTTPResponse &file_promise(const char *filename); // only sets the filename string inside the class
    HTTPResponse &file_attachment(string data, MIME type);
    HTTPResponse &file_attachment(const char *filename, MIME type, uint64_t begin_offset = 0);
    // use for when you want a certain action executed when finishing file transfer, e.g. removing temporary file when done
    HTTPResponse &file_attachment(const char *filename, MIME type, std::function<void(const char *, void *)> action, void *action_object);
    HTTPResponse &attach_file(MIME type);
    HTTPResponse &attach_file(MIME type, std::function<void(const char *, void *)> action, void *action_object);
    HTTPResponse &access_control(string control);
    HTTPResponse &content_range(uint64_t begin_offset); // only support begin offsets for now
    HTTPResponse &transfer_encoding(ENCODING encoding);
    class filesegment_iterator
    {
    private:
        size_t read_fragment_size;
        string filename;
        std::function<void(const char *, void *)> action;
        void *action_object;

    protected:
        struct data
        {
            size_t size = 0;
            char *fragment = NULL;
            size_t cur_frag_sent = 0;
        } file_data;
        std::ifstream *file = NULL;
        size_t max_fragment_size;
        uint64_t remaining = 0;

    public:
        filesegment_iterator(HTTPResponse *parent, size_t read_fragment_size, size_t max_fragment_size, uint64_t begin_offset);
        filesegment_iterator(HTTPResponse *parent, size_t read_fragment_size, uint64_t begin_offset);
        filesegment_iterator(filesegment_iterator &&f);
        filesegment_iterator(const filesegment_iterator &f) = default;
        virtual ~filesegment_iterator();
        bool has_next();
        virtual filesegment_iterator &operator++(int);
        data *operator->();
    };
    class zlib_compress_iterator : public filesegment_iterator
    {
    private:
        z_stream stream;
        char *compressed = NULL;
        int max_number_of_hex_digits; // 2 hex digits per byte, for the maximum size_t fragment size (see initializer list)
        int last_fragment_pointer_offset;
        // We don't init those to anything as it will override initialization lists
        size_t max_compressed_fragment_size; // Has additional http stuff (see comments in init_deflate implementation)
        size_t max_compressed_size;          // Maximum size zlib might output

        inline size_t init_deflate(size_t read_fragment_size, HTTPResponse::ENCODING encoding, int max_number_of_hex_digits);
        void deflate_chunk_fragment();

    public:
        zlib_compress_iterator(HTTPResponse *parent, size_t read_fragment_size, uint64_t begin_offset, HTTPResponse::ENCODING encoding);
        zlib_compress_iterator(zlib_compress_iterator &&f);
        zlib_compress_iterator(const zlib_compress_iterator &f) = default;
        ~zlib_compress_iterator() override;
        zlib_compress_iterator &operator++(int) override;
    };
    filesegment_iterator *begin_file_transfer(size_t fragment_size = 32 * (1 << 10) /* 32KB */);
    string get_filename();
    bool is_multifragment_transfer();
    bool is_promise_transfer();
    bool is_phony();
    const char *to_c_str();
    std::size_t size();
    friend ostream &operator<<(ostream &out, const HTTPResponse &r);
};
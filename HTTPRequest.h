#include <stdlib.h>
#include <stdint.h>

class HTTPRequest
{
public:
    enum class Method
    {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        PATCH,
        nil
    };

    enum class State
    {
        receive_header,
        process_header,
        receive_file,
        done
    };

    enum class ProcessStatus
    {
        incomplete,
        complete,
        error
    };

private:
    int fd;
    char *buffer;
    size_t buffer_size;

public:
    State cur_state;

    char *header_terminator = NULL;

    uint64_t remaining;
    char *content_length = NULL;

public:
    size_t header_size, read_size;
    uint64_t total_size;

    void reset(void);
    HTTPRequest(int fd, size_t buffer_size);
    HTTPRequest(HTTPRequest &r) = delete;
    HTTPRequest(HTTPRequest &&r);
    ~HTTPRequest();

    ProcessStatus receive_header();
    ProcessStatus process_header();
    ProcessStatus process_request();
    ProcessStatus process(void);

    char *get_data();

    Method get_method();
    int get_fd();
};

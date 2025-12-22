#pragma once

#include <stdint.h>
#include <string>
#include <fstream>

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
        receive_proxyv2_header,
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
    struct file_receive_data
    {
        std::string filename, boundary;
        std::ofstream *file = NULL;

        file_receive_data(std::string filename, std::string boundary);
        file_receive_data(file_receive_data &&f);
        file_receive_data(const file_receive_data &f) = default;
        ~file_receive_data();
    };

    struct proxy_hdr_v2
    {
        uint8_t sig[12]; /* hex 0D 0A 0D 0A 00 0D 0A 51 55 49 54 0A */
        uint8_t ver_cmd; /* protocol version and command */
        uint8_t fam;     /* protocol family and address */
        uint16_t len;    /* number of following bytes part of the header */
    };

    union proxy_addr
    {
        struct
        { /* for TCP/UDP over IPv4, len = 12 */
            uint32_t src_addr;
            uint32_t dst_addr;
            uint16_t src_port;
            uint16_t dst_port;
        } ipv4_addr;
        struct
        { /* for TCP/UDP over IPv6, len = 36 */
            uint8_t src_addr[16];
            uint8_t dst_addr[16];
            uint16_t src_port;
            uint16_t dst_port;
        } ipv6_addr;
        struct
        { /* for AF_UNIX sockets, len = 216 */
            uint8_t src_addr[108];
            uint8_t dst_addr[108];
        } unix_addr;
    };

    enum class ProxyFamily
    {
        tcp_ipv4 = 0x11,
        tcp_ipv6 = 0x21,
        unix_stream = 0x31
    };

    proxy_hdr_v2 proxyv2_header;
    proxy_addr proxyv2_address;
    bool proxy_processed = false;

    int fd;
    char *buffer;
    bool new_request = true;
    size_t buffer_size;
    std::string client_addr;

    file_receive_data *recv_file_data = NULL;

    ProcessStatus receive_proxyv2_data();

    ProcessStatus receive_header();
    ProcessStatus process_header();
    ProcessStatus process_request();

    ProcessStatus handle_receive_file_writes();
    ProcessStatus receive_file_data();

public:
    State cur_state;

    char *header_terminator = NULL;

    uint64_t remaining;
    char *content_length = NULL;

    size_t header_size, read_size;
    uint64_t total_size;

    void reset(void);
    HTTPRequest(int fd, size_t buffer_size, std::string client_addr);
    HTTPRequest(HTTPRequest &r) = delete;
    HTTPRequest(HTTPRequest &&r);
    ~HTTPRequest();

    ProcessStatus process(void);

    std::string get_client_addr();

    char *get_data();
    int get_fd();
    Method get_method();

    bool is_new();

    HTTPRequest::ProcessStatus start_file_reception(std::string filename, std::string boundary, char *read_data, size_t read_size, uint64_t remaining);
    bool is_receiving_file() { return recv_file_data != NULL; }
};

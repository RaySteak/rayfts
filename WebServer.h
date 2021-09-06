#pragma once
#include "common_utils.h"
#include "HTTPresponse.h"
#include "SessionCookie.h"
#include <unordered_map>
#include <vector>
#include <future>
#include <functional>
#define SERVER_DEBUG 1

using std::unordered_map;
using std::vector;

class WebServer
{
private:
    struct file_receive_data
    {
        string filename, boundary;
        std::ofstream *file = NULL;
        char *data[2] = {NULL, NULL};
        uint64_t remaining;
        size_t last_recv, prev_recv = 0;
        int current_buffer = 0;
        file_receive_data(string filename, string boundary, char *read_data, size_t read_size, uint64_t remaining)
            : filename(filename), boundary(boundary), remaining(remaining)
        {
            file = new std::ofstream(filename, std::ios::binary);
            data[0] = new char[WebServer::max_alloc];
            data[1] = new char[WebServer::max_alloc];
            memcpy(data[0], read_data, read_size);
            last_recv = read_size;
        }
        file_receive_data(file_receive_data &&f)
        {
            this->file = f.file;
            f.file = NULL;
            this->filename = f.filename;
            this->boundary = f.boundary;
            f.filename = "";
            this->data[0] = f.data[0];
            this->data[1] = f.data[1];
            f.data[0] = NULL;
            f.data[1] = NULL;
            this->remaining = f.remaining;
            this->last_recv = f.last_recv;
            this->prev_recv = f.prev_recv;
            this->current_buffer = f.current_buffer;
        }
        file_receive_data(const file_receive_data &f) = default;
        ~file_receive_data()
        {
            if (file)
            {
                delete file;
                delete data[0];
                delete data[1];
            }
        }
        char *get_next_buffer()
        {
            current_buffer = (current_buffer + 1) & 0b1;
            char *toret = data[current_buffer];
            return toret;
        }
    };

    string user, pass;
    int listenfd, newsockfd, port;
    char buffer[BUFLEN + 1];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, ret;
    socklen_t socklen;

    fd_set read_fds; // used for select
    fd_set tmp_fds;  // temporary set
    int fdmax;       // max val of read_fds

    unordered_map<string, Cookie *> cookies;
    unordered_map<int, std::pair<std::future<int>, HTTPresponse>> file_futures;

    unordered_map<int, HTTPresponse::filesegment_iterator> unsent_files;
    unordered_map<int, file_receive_data> unreceived_files;

    inline void init_server_params(int port, const char *user, const char *path);
    int send_exactly(int fd, const char *buffer, size_t count);
    int recv_exactly(int fd, char *buffer, size_t count, timeval t);
    int recv_http_header(int fd, char *buffer, int max, int &header_size);
    void close_connection(int fd, bool erase_from_sets);
    int process_http_header(int fd, char *buffer, int read_size, int header_size, char *&data, uint64_t &total);
    void process_cookies();
    void remove_from_read(int fd);
    void add_to_read(int fd);
    HTTPresponse process_http_request(char *data, int header_size, size_t read_size, uint64_t total_size, int fd);

    // int zip_folder(char *destination, char *path)
    // zips folder located at path/name and places it in destination
    // this function uses /bin/7z by default for ease of implementation,
    // it can be replaced by any function by using the 4-parameter constructor
    // this function must free the memory of the two parameters
    std::function<int(char *, char *)> zip_folder;

    const int timeout_milli = 100;
    static const size_t max_alloc = 16 * (1 << 10); // used for receive size

#ifdef SERVER_DEBUG
    const bool debug_mode = 1;
#else
    const bool debug_mode = 0;
#endif

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
        nil
    };

    WebServer(int port, const char *user, const char *path);
    WebServer(int port, const char *user, const char *path, std::function<int(char *, char *)> zip_folder);
    virtual ~WebServer();
    void run();
};
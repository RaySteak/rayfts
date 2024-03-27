#pragma once
#include "common_utils.h"
#include "HTTPresponse.h"
#include "SessionCookie.h"
#include "lock_writable_unordered_map.h"
#include "ping_device.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <future>
#include <functional>

using std::unordered_map;
using std::unordered_set;
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

    string user, pass_digest;
    int listenfd, newsockfd, port, udpfd;
    char buffer[BUFLEN + 1];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, ret;
    socklen_t socklen;

    fd_set read_fds; // used for select
    fd_set tmp_fds;  // temporary set
    int fdmax;       // max val of read_fds

    unordered_map<string, Cookie *> cookies;
    // tuple members are as follows: connections awaiting the file, future of file, response, temporary filename, size of final file (approximate for zips)
    unordered_map<string, std::tuple<unordered_set<int>, std::future<int>, HTTPresponse, string, uint64_t>> file_futures;
    unordered_map<string, std::pair<int, HTTPresponse>> downloading_futures;
    unordered_map<string, string> temp_to_path;
    unordered_map<int, string> fd_to_file_futures;
    lock_writable_unordered_map<string, pid_t> path_to_pid;

    unordered_map<int, HTTPresponse::filesegment_iterator *> unsent_files;
    unordered_map<int, file_receive_data> unreceived_files;

    // Remembering cut files for each session cookie for file moving
    unordered_map<string, std::pair<string, unordered_set<string>>> session_to_cut_files;

    unordered_set<string> iots;
    ping_device ping_machine{};

    inline void init_server_params(int port, const char *user, const char *pass_digest);
    int send_exactly(int fd, const char *buffer, size_t count);
    int recv_exactly(int fd, char *buffer, size_t count);
    int recv_http_header(int fd, char *buffer, int max, int &header_size);
    void close_connection(int fd, bool erase_from_sets);
    int process_http_header(int fd, char *buffer, int read_size, int header_size, char *&data, uint64_t &total);
    void process_cookies();
    void remove_from_read(int fd);
    void add_to_read(int fd);
    HTTPresponse queue_file_future(int fd, string temp_path, string folder_path, string folder_name, HTTPresponse::ENCODING encoding);
    HTTPresponse process_http_request(char *data, int header_size, size_t read_size, uint64_t total_size, int fd);

    // int zip_folder(char *destination, char *path)
    // zips folder located at path/name and places it in destination
    // this function uses /bin/7z by default for ease of implementation,
    // it can be replaced by any function by using the 4-parameter constructor
    // this function must free the memory of the two parameters
    std::function<int(char *, char *, WebServer *)> zip_folder; // TODO: use lambda capture instead of passing the server

    const int timeout_milli = 100;
    static const size_t max_alloc = 16 * (1 << 10); // used for receive size

#ifdef SERVER_DEBUG
    bool debug_mode = 1;
#else
    bool debug_mode = 0;
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
        PATCH,
        nil
    };

    WebServer(int port, const char *user, const char *path);
    WebServer(int port, const char *user, const char *path, std::function<int(char *, char *, WebServer *)> zip_folder);
    virtual ~WebServer();
    void run();
};
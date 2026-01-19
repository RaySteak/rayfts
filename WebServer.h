#pragma once
#include "common_utils.h"
#include "HTTPResponse.h"
#include "HTTPRequest.h"
#include "RateLimiter.h"
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
    string user, salt, salt_pass_digest;
    int listenfd, newsockfd, port, udpfd;
    char buffer[BUFLEN + 1];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, ret;
    socklen_t socklen;

    fd_set read_fds; // used for select
    fd_set tmp_fds;  // temporary set
    int fdmax;       // max val of read_fds

    // map of fd to client address and HTTPRequest
    unordered_map<int, HTTPRequest> connections;
    RateLimiter rate_limiter;

    unordered_map<string, Cookie *>
        cookies;
    // tuple members are as follows: connections awaiting the file, future of file, response, temporary filename, size of final file (approximate for zips)
    unordered_map<string, std::tuple<unordered_set<int>, std::future<int>, HTTPResponse, string, uint64_t>> file_futures;
    unordered_map<string, std::pair<int, HTTPResponse>> downloading_futures;
    unordered_map<string, string> temp_to_path;
    unordered_map<int, string> fd_to_file_futures;
    lock_writable_unordered_map<string, pid_t> path_to_pid;

    unordered_map<int, HTTPResponse::filesegment_iterator *> unsent_files;

    // Remembering cut files for each session cookie for file moving
    unordered_map<string, std::pair<string, unordered_set<string>>> session_to_cut_files;

    unordered_set<string> iots;
    ping_device ping_machine{};

    vector<string> public_paths;

    inline void init_server_params(int port, const char *user, const char *salt, const char *salt_pass_digest);
    int send_exactly(int fd, const char *buffer, size_t count);
    int recv_exactly(int fd, char *buffer, size_t count);
    // int recv_http_header(int fd, char *buffer, int max, int &header_size);
    void close_connection(int fd, bool erase_from_sets);
    int process_http_header(int fd, char *buffer, int read_size, int header_size, char *&data, uint64_t &total);
    void process_cookies();
    void remove_from_read(int fd);
    void add_to_read(int fd);
    HTTPResponse queue_file_future(int fd, string temp_path, string folder_path, string folder_name, HTTPResponse::ENCODING encoding);
    HTTPResponse process_http_request(HTTPRequest &request);

    // int zip_folder(char *destination, char *path)
    // zips folder located at path/name and places it in destination
    // this function uses /bin/7z by default for ease of implementation,
    // it can be replaced by any function by using the 5-parameter constructor of WebServer
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
    class ConfigFiles
    {
    public:
        static const char *const wol_list;
        static const char *const public_paths;
    };

    WebServer(int port, const char *user, const char *salt, const char *salt_pass_digest, RateLimiter rate_limiter = RateLimiter());
    WebServer(int port, const char *user, const char *salt, const char *salt_pass_digest, std::function<int(char *, char *, WebServer *)> zip_folder, RateLimiter rate_limiter = RateLimiter());
    virtual ~WebServer();
    void run();
};
#pragma once
#include "common_utils.h"
#include "HTTPresponse.h"
#include "SessionCookie.h"
#include <unordered_map>
#include <vector>

using std::unordered_map;
using std::vector;

class WebServer
{
private:
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

    int send_exactly(int fd, const char *buffer, int count);
    int recv_http_header(int fd, char *buffer, int max, int &header_size);
    void close_connection(int fd);
    int process_http_header(int fd, char *buffer, int read_size, int header_size, char *&data);
    void process_cookies();
    HTTPresponse process_http_request(char *data, int header_size, int total_size);

    const int timeout_secs = 0;
    const int timeout_micro = 100000;

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
    void run();
    ~WebServer();
};
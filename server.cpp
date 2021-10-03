#include "WebServer.h"
#include <new>

void usage(const char *exec)
{
    std::cerr << "Usage: " << exec << " server_port user password\n";
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    if (argc != 4) // incorrect arg count
    {
        usage(argv[0]);
        return -1;
    }

    int port = atoi(argv[1]);
    if (port == 0) // port is either 0 or not a number
    {
        usage(argv[0]);
        return 2;
    }
    if (port > MAX_PORT || port < 0) // port number is too high or is negative
    {
        std::cerr << "Port " << port << " out of range\n";
        return -1;
    }

    WebServer *server = new (std::nothrow) WebServer(port, argv[2], argv[3]);
    DIE(server == NULL, "new");

    server->run();

    delete server;

    return 0;
}
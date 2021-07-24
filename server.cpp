#include "WebServer.h"
#include <new>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " server_port user password\n";
        return -1;
    }

    int port = atoi(argv[1]);
    DIE(port == 0, "atoi");

    WebServer *server = new (std::nothrow) WebServer(port, argv[2], argv[3]);
    DIE(server == NULL, "new");

    server->run();

    delete server;

    return 0;
}
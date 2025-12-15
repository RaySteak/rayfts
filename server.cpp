#include "WebServer.h"
#include <new>
#include "execinfo.h"

void usage(const char *exec)
{
    std::cerr << "Usage: " << exec << " server_port user salt hash\n";
}

void handler(int sig)
{
    void *array[100];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 100);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGABRT, handler);
    if (argc != 5) // incorrect arg count
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

    WebServer *server = new (std::nothrow) WebServer(port, argv[2], argv[3], argv[4]);
    DIE(server == NULL, "new");

    server->run();

    delete server;

    return 0;
}
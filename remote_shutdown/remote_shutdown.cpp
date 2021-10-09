#include "remote_shutdown.h"

#if __has_include("../common_utils.h")
#include "../common_utils.h"
#else
#include "common_utils.h"
#endif

#ifndef WIN32
#include <spawn.h>
#endif

int main()
{
    if (strlen(REMOTE_SHUTDOWN_KEYWORD) > BUFLEN)
    {
        std::cerr << "Keyword size too long\n";
        exit(1);
    }
    int listenfd, newsockfd, port;
    char buffer[BUFLEN + 1];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, ret;
    socklen_t socklen;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd < 0, "socket");

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(REMOTE_SHUTDOWN_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int option = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int));
    DIE(ret < 0, "reuse_addr");

    ret = bind(listenfd, (sockaddr *)&serv_addr, sizeof(sockaddr));
    DIE(ret < 0, "bind");

    ret = listen(listenfd, MAX_CLIENTS);
    DIE(ret < 0, "listen");

    pollfd pfd{.fd = listenfd, .events = POLLIN, .revents = 0};
    ret = poll(&pfd, 1, -1);
    DIE(ret < 0, "poll");

    int fd = accept(listenfd, (sockaddr *)&cli_addr, &socklen);

    n = recv(fd, buffer, strlen(REMOTE_SHUTDOWN_KEYWORD), NULL); //TODO: change to recv exactly;
    buffer[n + 1] = 0;
    if (!strcmp(buffer, REMOTE_SHUTDOWN_KEYWORD))
    {
#ifdef WIN32
        system("shutdown /s /t 0");
#else
        char argv0[] = "/usr/sbin/poweroff";
        char *const argv[] = {argv0, NULL};
        pid_t pid;
        posix_spawn(&pid, argv0, NULL, NULL, argv, NULL);
#endif
    }
    // if something else than the keyword is sent, maybe another application wants to use this port,
    // so we exit the program to allow it. Recommended course of action is to change the port macro in remote_shutdown.h
    return 0;
}
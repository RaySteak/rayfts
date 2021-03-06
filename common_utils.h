#pragma once
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include <signal.h>
#include <poll.h>
#else
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif

#define DIE(assertion, call_description)                       \
    do                                                         \
    {                                                          \
        if (assertion)                                         \
        {                                                      \
            fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__); \
            perror(call_description);                          \
            exit(EXIT_FAILURE);                                \
        }                                                      \
    } while (0)

#define startcmp(s, w) strncmp(s, w, strlen(w))

#define BUFLEN 8192
#define MAX_CLIENTS 5
#define MAX_PORT 65535

int send_udp(int fd, const char *buffer, size_t count, const char *address, uint16_t port);
int recv_udp(int fd, char *buffer, size_t max_count, const char *address, uint16_t port, unsigned int timeout_ms);
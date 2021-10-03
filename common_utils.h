#pragma once
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include <signal.h>
#include <poll.h>

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
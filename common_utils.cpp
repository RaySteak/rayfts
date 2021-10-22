#include "common_utils.h"

int send_udp(int fd, const char *buffer, size_t count, const char *address, uint16_t port)
{
    sockaddr_in cli_addr;
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(port);
    inet_aton(address, &cli_addr.sin_addr);
    return sendto(fd, buffer, count, 0, (sockaddr *)&cli_addr, sizeof(cli_addr));
}

int recv_udp(int fd, char *buffer, size_t max_count, const char *address, uint16_t port, unsigned int timeout_ms)
{
    sockaddr_in cli_addr;
    socklen_t socklen;
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    DIE(ret < 0, "setsockopt");
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(port);
    inet_aton(address, &cli_addr.sin_addr);
    return recvfrom(fd, buffer, max_count, 0, (sockaddr *)&cli_addr, &socklen);
}
#include "common_utils.h"

int recv_at_least(int fd, char *buffer, int max, int min)
{
    int nr = 0;
    while (nr < min)
    {
        int n = recv(fd, buffer + nr, max, 0);
        if (n < 0)
            return n;
        if (n == 0)
            return 0;
        nr += n;
        max -= n;
    }
    return nr;
}

int send_exactly(int fd, char *buffer, int count)
{
    int nr = 0;
    while (count)
    {
        int n = send(fd, buffer + nr, count, 0);
        if (n < 0)
            return n;
        nr += n;
        count -= n;
    }
    return nr;
}
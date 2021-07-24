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

int recv_packet(int fd, char *buffer)
{
    int n = 0;
    int aux_n = recv_at_least(fd, buffer, BUFLEN, sizeof(tcp_message_header));
    if (aux_n < 0)
        return aux_n;
    if (aux_n == 0)
        return 0;
    n += aux_n;
    int packet_length = sizeof(tcp_message_header) + ntohs(((tcp_message_header *)buffer)->length);
    if (packet_length > aux_n)
    {
        aux_n = recv_at_least(fd, buffer + aux_n, BUFLEN - aux_n, packet_length - aux_n);
        if (aux_n < 0)
            return aux_n;
        n += aux_n;
    }
    return n;
}

int get_next_packet_and_receive(int &n, char *ptr, int fd, char *buffer)
{
    tcp_message_header *msg_hdr = (tcp_message_header *)ptr;
    int dif = n - (sizeof(tcp_message_header) + ntohs(msg_hdr->length));
    if (dif > 0)
    {
        tcp_message_header *newmsg_hdr = (tcp_message_header *)(ptr + (n - dif));
        if (dif < (int)sizeof(tcp_message_header) || (int)(sizeof(tcp_message_header) + ntohs(newmsg_hdr->length)) > dif) // less than a header or packet is not complete
        {
            char aux_buffer[2 * BUFLEN];
            memcpy(aux_buffer, ptr + n - dif, dif);
            newmsg_hdr = (tcp_message_header *)aux_buffer;
            int new_n = 0, delta = 0;
            if (dif < (int)sizeof(tcp_message_header))
            {
                delta = recv_at_least(fd, aux_buffer + dif, BUFLEN - dif, sizeof(tcp_message_header) - dif);
                DIE(delta < 0, "recv");
                new_n += delta;
            }
            if (new_n < (int)(sizeof(tcp_message_header) + ntohs(newmsg_hdr->length)))
            {
                delta = recv_at_least(fd, aux_buffer + dif + delta, BUFLEN - dif - delta, sizeof(tcp_message_header) + ntohs(newmsg_hdr->length) - dif - delta);
                DIE(delta < 0, "recv");
                new_n += delta;
            }
            DIE(new_n < 0, "recv");
            memcpy(buffer, aux_buffer, dif + new_n);
            n = new_n + dif;
            return 0;
        }
        int old_n = n;
        n = dif;
        return old_n - dif;
    }
    return -1;
}
#include "wake_on_lan.h"
#include <sstream>

#define MAGIC_COUNT 16
#define BROADCAST 0xffffffff
#define WOL_PORT 7

using namespace wol;

wake_on_lan::wake_on_lan(std::string address)
{
    for (size_t i = 0; i < address.size() - 1; i += 3)
    {
        std::stringstream octet;
        octet.str(address.substr(i, 2));
        int c;
        octet >> std::hex >> c;
        mac += (char)c;
    }
    magic = std::string(6, 0xff);
    for (int i = 0; i < MAGIC_COUNT; i++) // add required target address for magic packet
        magic += mac;
}

bool wake_on_lan::awaken()
{
    int ret, optval;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    optval = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    if (ret < 0)
        return false;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WOL_PORT);
    addr.sin_addr.s_addr = BROADCAST;

    ret = sendto(sockfd, magic.c_str(), magic.size(), 0, (sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
        return false;
    return true;
}

#include "wake_on_lan.h"
#include <sstream>
#include <fstream>

#define MAGIC_COUNT 16
#define BROADCAST 0xffffffff
#define WOL_PORT 7
#define LISTS_LINE_MAX 200

using namespace wol;

void wake_on_lan::init()
{
    for (size_t i = 0; i < mac_readable.size() - 1; i += 3)
    {
        std::stringstream octet;
        octet.str(mac_readable.substr(i, 2));
        int c;
        octet >> std::hex >> c;
        mac += (char)c;
    }
    magic = std::string(6, 0xff);
    for (int i = 0; i < MAGIC_COUNT; i++) // add required target address for magic packet
        magic += mac;
}

wake_on_lan::wake_on_lan(std::string address)
{
    this->mac_readable = address;
    init();
}

wake_on_lan::wake_on_lan(std::string address, std::string device_name)
{
    this->device_name = device_name;
    this->mac_readable = address;
    init();
}

wake_on_lan::wake_on_lan(std::string address, std::string device_name, std::string ip)
{
    this->device_name = device_name;
    this->mac_readable = address;
    this->ip = ip;
    init();
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

std::vector<wake_on_lan> wake_on_lan::parse_list(const char *filename)
{
    std::vector<wake_on_lan> list;
    char buffer[LISTS_LINE_MAX];
    std::ifstream file(filename, std::ios::in);
    while (file.getline(buffer, LISTS_LINE_MAX))
    {
        char *mac = strtok(buffer, " ");       // for wake on lan
        char *ip = strtok(NULL, " ");          // for checking if computer is awake
        char *device_name = strtok(NULL, " "); // also the username for ssh-ing into machine
        list.push_back({mac, device_name, ip});
    }
    return list;
}

std::string &wake_on_lan::get_device_name()
{
    return device_name;
}

std::string wake_on_lan::get_mac_readable()
{
    return mac_readable;
}

std::string wake_on_lan::get_ip()
{
    return ip;
}
#pragma once

#include "common_utils.h"
#include <string>
#include <vector>

namespace wol
{
    class wake_on_lan
    {
    private:
        std::string mac, magic, device_name, mac_readable, ip;
        void init();

    public:
        wake_on_lan(std::string address); // given as XX:XX:...
        wake_on_lan(std::string address, std::string device_name);
        wake_on_lan(std::string address, std::string device_name, std::string ip);
        bool awaken();
        static std::vector<wake_on_lan> parse_list(const char *filename);
        std::string &get_device_name();
        std::string get_mac_readable();
        std::string get_ip();
    };
};

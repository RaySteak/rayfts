#pragma once

#include "common_utils.h"
#include <string>
#include <vector>

namespace wol
{
    class wake_on_lan
    {
    private:
        std::string mac, magic, device_name, mac_readable;

    public:
        wake_on_lan(std::string address, std::string device_name); // given as XX:XX:...
        bool awaken();
        static std::vector<wake_on_lan> parse_list(const char *filename);
        std::string &get_device_name();
        std::string get_mac_readable();
    };
};

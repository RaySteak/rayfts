#pragma once

#include <string>
#include "common_utils.h"

namespace wol
{
    class wake_on_lan
    {
    private:
        std::string mac;
        std::string magic;

    public:
        wake_on_lan(std::string address); // given as XX:XX:...
        bool awaken();
    };
};

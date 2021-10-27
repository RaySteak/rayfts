// A device class that checks the state of multiple connected devices (not a class that pings a device :) )

#pragma once
#include <future>
#include <tuple>
#include <vector>
#include <string>
#include <mutex>
#include <vector>
#include <time.h>
#include <string>
#include "common_utils.h"

class ping_device
{
public:
    enum class type
    {
        arduino, // use the defined arduino protocol  to check state
        rawping  // use the fping program to check state
    };

    ping_device();
    ~ping_device();
    void add_device(type t, std::string ip, uint16_t port = 0, unsigned int timeout_ms = 10);
    //void remove_device(std::string ip);
    void clear_devices();
    void ping_devices();
    void wind(time_t secs);
    std::string get_states();
    bool is_running();

private:
    char buffer[BUFLEN];
    int sockfd;
    std::vector<std::tuple<std::string, type, uint16_t, unsigned int>> devices;
    std::string states;
    std::mutex time_mutex, states_mutex, devices_mutex;
    time_t last_wind_time = 0, cur_wind = 0;
    std::future<void> ping;
};
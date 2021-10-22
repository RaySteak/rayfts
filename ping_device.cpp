#include "ping_device.h"
#include "arduino/arduino_constants.h"
#include <spawn.h>
#include <sys/wait.h>
#include <functional>
#include <fcntl.h>

using namespace arduino_constants;

ping_device::ping_device()
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfd < 0, "socket");
}

ping_device::~ping_device()
{
    int ret = close(sockfd);
    DIE(ret < 0, "close");
}

void ping_device::add_device(type t, std::string ip, uint16_t port, unsigned int timeout_ms)
{
    devices.push_back({ip, t, port, timeout_ms});
    states += '0';
}

/*void ping_device::remove_device(std::string ip)
{
    devices.erase(ip);
}*/

void ping_device::clear_devices()
{
    std::lock_guard<std::mutex> lck_grd(devices_mutex);
    devices.clear();
}

void ping_device::ping_devices()
{
    while (true)
    {
        int i = 0;
        unsigned int max_timeout = 0;
        std::lock_guard<std::mutex> lck_grd(devices_mutex);
        for (auto [ip, t, port, timeout_ms] : devices)
        {
            max_timeout = std::max(max_timeout, timeout_ms);
            switch (t)
            {
            case type::arduino:
            {
                send_udp(sockfd, GET_STATE, strlen(GET_STATE), ip.c_str(), PORT);
                int n = recv_udp(sockfd, buffer, BUFLEN, ip.c_str(), PORT, timeout_ms);
                std::lock_guard<std::mutex> lck_grd(states_mutex);
                states[i] = n > 0 ? (buffer[0] == '1' ? '1' : '0') : '0'; // might seem redundant but this way we return 0 for altered messages
            }
            break;
            case type::rawping:
            {
                posix_spawn_file_actions_t action;
                posix_spawn_file_actions_init(&action);
                posix_spawn_file_actions_addopen(&action, STDOUT_FILENO, "/dev/null", O_WRONLY | O_APPEND, 0);
                posix_spawn_file_actions_addopen(&action, STDERR_FILENO, "/dev/null", O_WRONLY | O_APPEND, 0);

                pid_t pid;
                char argv0[] = "/bin/fping", argv1[] = "-c1", argv2[15] = "-t", *argv3 = strdup(ip.c_str());
                strcat(argv2, std::to_string(timeout_ms).c_str());
                char *const argv[] = {argv0, argv1, argv2, argv3, NULL};
                int status = posix_spawn(&pid, argv0, &action, NULL, argv, NULL);
                free(argv3);
                posix_spawn_file_actions_destroy(&action);
                states_mutex.lock();
                if (status)
                    states[i] = 'X';
                states_mutex.unlock();
                waitpid(pid, &status, 0);
                std::lock_guard<std::mutex> lck_grd(states_mutex);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                    states[i] = '1';
                else
                    states[i] = '0';
            }
            }
            i++;
        }
        time_mutex.lock();
        if (time(0) - last_wind_time >= cur_wind)
            break;
        time_mutex.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(max_timeout));
    }
    time_mutex.unlock();
}

void ping_device::wind(time_t secs)
{
    time_mutex.lock();
    last_wind_time = time(0);
    cur_wind = secs;
    time_mutex.unlock();
    bool should_launch = false;
    try
    {
        if (ping.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            should_launch = true;
    }
    catch (const std::exception &e)
    {
        should_launch = true;
    }

    if (should_launch)
        ping = std::async(std::launch::async, &ping_device::ping_devices, this);
}

std::string ping_device::get_states()
{
    std::lock_guard<std::mutex> lck_grd(states_mutex);
    return states;
}
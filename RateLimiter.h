#pragma once

#include <stdint.h>
#include <unordered_map>
#include <string>
#include <chrono>
#include <mutex>

class RateLimiter
{
private:
    std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> client_tokens;
    int max_requests;
    int64_t refill_interval_ms;
    int64_t client_cleanup_interval_ms;

    std::chrono::steady_clock::time_point last_cleanup;

    void cleanup_old_clients(void);

public:
    RateLimiter(int max_requests = 100, int refill_interval_ms = 10, int client_cleanup_interval_ms = 1000000);
    ~RateLimiter();

    bool take_token(std::string client_addr);
};

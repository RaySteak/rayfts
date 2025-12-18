#include "RateLimiter.h"

RateLimiter::RateLimiter(int max_requests, int refill_interval_ms, int client_cleanup_interval_ms)
    : max_requests(max_requests), refill_interval_ms(refill_interval_ms), client_cleanup_interval_ms(client_cleanup_interval_ms)
{
    last_cleanup = std::chrono::steady_clock::now();
}

RateLimiter::~RateLimiter()
{
}

void RateLimiter::cleanup_old_clients(void)
{
    auto now = std::chrono::steady_clock::now();
    if ((now - last_cleanup) < std::chrono::milliseconds(client_cleanup_interval_ms))
        return;

    last_cleanup = now;

    for (auto it = client_tokens.begin(); it != client_tokens.end();)
    {
        auto last_access = it->second.second;
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_access);
        if (diff.count() > (int64_t)max_requests * refill_interval_ms)
            it = client_tokens.erase(it);
        else
            it++;
    }
}

bool RateLimiter::take_token(uint32_t client_ip)
{
    cleanup_old_clients();
    auto it = client_tokens.find(client_ip);

    if (it == client_tokens.end())
    {
        auto now = std::chrono::steady_clock::now();
        client_tokens.insert({client_ip, {max_requests - 1, now}});
        auto it2 = client_tokens.find(client_ip);
        return true;
    }

    int num_remaining = it->second.first;
    auto last_access = it->second.second;
    auto now = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_access);

    int tokens_to_add = diff.count() / refill_interval_ms;
    int64_t remaining_ms = (int64_t)diff.count() % refill_interval_ms;

    num_remaining = std::min(max_requests - 1, num_remaining + tokens_to_add - 1);

    it->second.first = num_remaining;
    if (it->second.first < 0)
    {
        it->second.first = 0;
        it->second.second = now;
        return false;
    }

    if (it->second.first == max_requests - 1)
        it->second.second = now;
    else
        it->second.second = now - std::chrono::milliseconds(remaining_ms);

    return true;
}

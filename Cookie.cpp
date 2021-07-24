#include "Cookie.h"
#include <iostream>

const string Cookie::alphabet = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789";

Cookie::Cookie()
{
    std::uint32_t seed;
    sysrandom(&seed, sizeof(seed));
    std::mt19937 rng(seed);
    std::uniform_int_distribution<> dis(0, alphabet.size() - 1);
    for (int i = 0; i < COOKIE_LENGTH; i++)
        cookie += alphabet[dis(rng)];
}

Cookie::~Cookie()
{
}

void Cookie::sysrandom(void *dst, size_t len)
{
    char *buffer = (char *)dst;
    ifstream stream("/dev/urandom", std::ios_base::binary | std::ios_base::in);
    stream.read(buffer, len);
}
string Cookie::val()
{
    return cookie;
}

string Cookie::expiry()
{
    return "";
}
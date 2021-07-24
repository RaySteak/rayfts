#pragma once
#include <time.h>
#include <string>
#include <fstream>
#include <random>

#define COOKIE_LENGTH 10

using std::ifstream;
using std::string;

class Cookie
{
protected:
    static const string alphabet;
    string cookie;
    void sysrandom(void *dst, size_t len);

public:
    Cookie();
    virtual ~Cookie();
    virtual bool is_expired() = 0;
    virtual void over() = 0;
    virtual void update() = 0;
    virtual string identifier() = 0;
    virtual string expiry();
    string val();
};
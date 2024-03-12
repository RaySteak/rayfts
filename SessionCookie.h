#pragma once
#include "Cookie.h"

#define SESSION_LENGTH 15

class SessionCookie : public Cookie
{
private:
    bool is_over;
    time_t left, last_updated;

public:
    bool is_expired();
    void over();
    void update();
    void renew();
    string expiry();
    string identifier();

    SessionCookie();
};
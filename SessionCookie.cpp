#include "SessionCookie.h"

bool SessionCookie::is_expired()
{
    return is_over;
}

void SessionCookie::over()
{
    is_over = true;
}

string SessionCookie::identifier()
{
    return "sessionId";
}

SessionCookie::SessionCookie()
{
    left = SESSION_LENGTH * 60;
    last_updated = time(0);
}

void SessionCookie::update()
{
    time_t now = time(0);
    left -= now - last_updated;
    last_updated = now;
    is_over = left <= 0 ? true : false;
}

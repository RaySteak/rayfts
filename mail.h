#pragma once

#include "common_utils.h"
#include <string>
#include <vector>

namespace mail
{
    class Mail
    {
    private:
        std::string sender_address, sender, recipient_address, recipient, mail_server, smtp_user, smtp_pass;
        const uint16_t default_smtp_port = 2525;

        void init();

    public:
        Mail(std::string sender_address, std::string sender, std::string recipient_address, std::string recipient, std::string mail_server, std::string smtp_user = "", std::string smtp_pass = "");
        bool send(std::string subject, std::string body);
        static std::vector<Mail> parse_list(const char *filename);
        static void add_to_list(Mail mail, const char *filename);
    };
};

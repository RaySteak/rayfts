#include "mail.h"
#include <curl/curl.h>
#include <fstream>
#include <cstdint>

using namespace mail;

Mail::Mail(std::string sender_address, std::string sender, std::string recipient_address, std::string recipient, std::string mail_server, std::string smtp_user, std::string smtp_pass)
{
    this->sender_address = sender_address;
    this->sender = sender;
    this->recipient_address = recipient_address;
    this->recipient = recipient;
    this->mail_server = mail_server;
    this->smtp_user = smtp_user;
    this->smtp_pass = smtp_pass;
}

std::vector<Mail> Mail::parse_list(const char *filename)
{
    std::vector<Mail> mails;
    std::ifstream mail_list_file(filename);
    std::string line;
    while (std::getline(mail_list_file, line))
    {
        char *sender_address = strtok((char *)line.c_str(), ",");
        char *sender_name = strtok(NULL, ",");
        char *recipient_address = strtok(NULL, ",");
        char *recipient_name = strtok(NULL, ",");
        char *mail_server = strtok(NULL, ",");
        char *smtp_user = strtok(NULL, ",");
        char *smtp_pass = strtok(NULL, ",");
        if (sender_address == NULL || sender_name == NULL || recipient_address == NULL || recipient_name == NULL || mail_server == NULL)
            continue;
        if ((smtp_user != NULL && smtp_pass == NULL))
            continue;
        mails.emplace_back(sender_address, sender_name, recipient_address, recipient_name, mail_server, smtp_user ? smtp_user : "", smtp_pass ? smtp_pass : "");
    }
    return mails;
}

void Mail::add_to_list(Mail mail, const char *filename)
{
    std::ofstream mail_list_file(filename, std::ios::app);
    mail_list_file << mail.sender_address << ',' << mail.sender << ',' << mail.recipient_address << ',' << mail.recipient << ',' << mail.mail_server << ',' << mail.smtp_user << ',' << mail.smtp_pass << '\n';
}

struct MailSendData
{
    std::string data;
    size_t offset = 0;
};

size_t mail_read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    MailSendData *send_data = (MailSendData *)userdata;
    size_t to_copy = size * nmemb < (send_data->data.size() - send_data->offset) ? size * nmemb : send_data->data.size() - send_data->offset;
    memcpy(ptr, send_data->data.c_str() + send_data->offset, to_copy);
    send_data->offset += to_copy;
    return to_copy;
}

bool Mail::send(std::string subject, std::string body)
{
    CURL *curl;
    CURLcode res = CURLE_OK;

    curl = curl_easy_init();
    if (!curl)
        return false;

    // std::string mail_server_address(mail_server);
    std::uint16_t mail_server_port = default_smtp_port;
    std::string mail_server_address = mail_server;

    if (mail_server_address.find("://") == std::string::npos)
        mail_server_address = "smtp://" + mail_server_address;

    size_t port_delimiter_pos = mail_server_address.find_last_of(':');
    if (port_delimiter_pos != std::string::npos && port_delimiter_pos > mail_server_address.find("://") + 3) // check if there is a port specified and that the colon isn't part of the protocol
    {
        mail_server_port = static_cast<std::uint16_t>(std::stoi(mail_server_address.substr(port_delimiter_pos + 1)));
        mail_server_address = mail_server_address.substr(0, port_delimiter_pos);
    }
    std::cout << mail_server << ' ' << mail_server_port << '\n';

    struct curl_slist *recipients = NULL;
    curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_user.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_pass.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, mail_server.c_str());
    curl_easy_setopt(curl, CURLOPT_PORT, mail_server_port);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, sender_address.c_str());
    recipients = curl_slist_append(recipients, recipient_address.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    std::string data = "To: " + recipient + " <" + recipient_address + ">\r\nFrom: " + sender + " <" + sender_address + ">\r\nSubject: " + subject + "\r\n\r\n" + body;
    MailSendData send_data;
    send_data.data = data;
    curl_easy_setopt(curl, CURLOPT_READDATA, &send_data);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(send_data.data.size()));
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, mail_read_callback);
    res = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

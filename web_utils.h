#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string.h>
#include "HTTPresponse.h"

namespace web_utils
{
    std::unordered_set<std::string> split_by_separators(char *content, const char *separators);
    std::unordered_map<std::string, std::string> get_content_fields(char *content, const char *is, const char *separators);
    std::string human_readable(uint64_t size);
    inline std::string add_image(std::string image)
    {
        return "<img src=\"/images/" + image + "\" height=\"20\" width=\"20\" alt=\"N/A\">";
    }
    inline std::string add_table_image(std::string image)
    {
        return "<td>" + add_image(image) + "</td>";
    }
    inline std::string image_attribute(std::string image)
    {
        return "style=\"background-image: url('/images/" + image + "')\"";
    }
    std::string parse_webstring(std::string name, bool replace_plus);
    HTTPresponse::MIME guess_mime_type(std::string filepath);
    void ping(int sockfd, struct in_addr ip_addr, int count);
    bool check_name(std::string name, const char *not_allowed = "&/%~="); //default list of characters not accepted
    uint64_t get_folder_size(std::string folder_path);
    std::string get_action_and_truncate(std::string &url, bool check_exists = true);
    std::string generate_folder_html(std::string path, const unsigned int max_name_length = 30);
};
#pragma once
#include <unordered_map>
#include <string>
#include <string.h>
#include "HTTPresponse.h"

namespace web_utils
{
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
    std::string parse_webstring(std::string name, bool replace_plus);
    HTTPresponse::MIME guess_mime_type(std::string filepath);
};
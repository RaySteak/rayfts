#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string.h>
#include "HTTPresponse.h"

namespace web_utils
{
    enum class MatchType
    {
        none = 0,
        exact,
        prefix,
    };

    std::unordered_set<std::string> split_by_separators(char *content, const char *separators);
    std::unordered_set<std::string> split_by_separators(string content, const char *separators);
    std::unordered_map<std::string, std::string> get_content_fields(char *content, const char *is, const char *separators);
    std::string parse_webstring(std::string name, bool replace_plus); // overwrites the string upon parsing
    string encode_webstring(string name);
    HTTPresponse::MIME guess_mime_type(std::string filepath);
    void ping(int sockfd, struct in_addr ip_addr, int count);
    // old default not_allowed value: "/%~=". TODO: check if there are any new problems with new list
    bool check_name(std::string name, const char *not_allowed = "/"); // default list of characters not accepted
    uint64_t get_folder_size(std::string folder_path);
    std::string get_action_and_truncate(std::string &url, bool check_exists = true);
    std::string generate_directory_data(string path, std::vector<string> &public_paths, bool cur_public);
    MatchType check_path_matches(std::string path, std::vector<std::string> &match_list);
};
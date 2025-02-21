#include "web_utils.h"
#include "common_utils.h"
#include <sstream>
#include <boost/filesystem.hpp>
#include <unordered_set>
#include <set>

namespace fs = boost::filesystem;

using std::set;
using std::string;
using std::to_string;
using std::unordered_map;
using std::unordered_set;

unordered_set<string> web_utils::split_by_separators(char *content, const char *separators)
{
    unordered_set<string> fields;
    for (char *p = strtok(content, separators); p; p = strtok(NULL, separators))
        fields.insert({p});
    return fields;
}

unordered_set<string> web_utils::split_by_separators(string content, const char *separators)
{
    char *copy = strdup(content.c_str());
    unordered_set<string> fields = split_by_separators(copy, separators);
    free(copy);
    return fields;
}

unordered_map<string, string> web_utils::get_content_fields(char *content, const char *is, const char *separators)
{
    unordered_map<string, string> fields;
    for (char *p = strtok(content, separators); p; p = strtok(NULL, separators))
    {
        char *eq = strstr(p, is);
        if (!eq)
            return unordered_map<string, string>();
        eq[0] = 0;
        string field = p, val = eq + strlen(is);
        fields.insert({field, val});
    }
    return fields;
}

string web_utils::parse_webstring(string name, bool replace_plus)
{
    const char hex_digits[] = "0123456789ABCDEF";
    size_t pos = 0;
    if (replace_plus)
    {
        while ((pos = name.find('+', pos)) != string::npos)
            name[pos] = ' ';
    }
    pos = 0;
    while ((pos = name.find('%', pos)) != string::npos)
    {
        if (name.size() - pos <= 2)
        {
            pos++;
            continue;
        }
        char c1 = name[pos + 1], c2 = name[pos + 2];
        if (!strchr(hex_digits, c1) || !strchr(hex_digits, c2) || (c1 == '0' && c2 == '0'))
        {
            pos++;
            continue;
        }
        std::stringstream character;
        character.str(name.substr(pos + 1, 2));
        name.erase(pos, 3);
        int c;
        character >> std::hex >> c;
        name.insert(name.begin() + pos, (char)c);
    }
    pos = 0;
    return name;
}

string web_utils::encode_webstring(string name)
{
    const char hex_digits[] = "0123456789ABCDEF";
    string encoded;
    for (auto c : name)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += c;
        }
        else
        {
            encoded += '%';
            encoded += hex_digits[c >> 4];
            encoded += hex_digits[c & 0xF];
        }
    }
    return encoded;
}

HTTPresponse::MIME web_utils::guess_mime_type(string filepath)
{
    size_t pos = filepath.find_last_of('.');
    string extension = filepath.substr(pos + 1);

    if (extension == "png")
        return HTTPresponse::MIME::png;
    if (extension == "jpeg" || extension == "jpg")
        return HTTPresponse::MIME::jpeg;
    if (extension == "js")
        return HTTPresponse::MIME::javascript;
    if (extension == "html")
        return HTTPresponse::MIME::html;
    if (extension == "css")
        return HTTPresponse::MIME::css;
    if (extension == "mp4")
        return HTTPresponse::MIME::mp4;
    if (extension == "mkv")
        return HTTPresponse::MIME::mkv;
    if (extension == "pdf")
        return HTTPresponse::MIME::pdf;
    if (extension == "vs")
        return HTTPresponse::MIME::text;
    if (extension == "fs")
        return HTTPresponse::MIME::text;
    // can't guess type, just return byte stream
    return HTTPresponse::MIME::octet_stream;
}

bool web_utils::check_name(string name, const char *not_allowed)
{
    // Don't allow ".." by default to prevent path injection. Also don't allow the name to be "." (curent directory)
    if (name.length() > 255 || name.find("..") != std::string::npos || name == ".")
        return false;
    for (auto &c : name)
    {
        if (strchr(not_allowed, c))
            return false;
    }
    return true;
}

uint64_t web_utils::get_folder_size(string folder_path)
{
    uint64_t size = 0LL;
    for (fs::recursive_directory_iterator it(folder_path), end; it != end; it++)
    {
        auto entry = fs::directory_entry(it->path());
        if (fs::is_regular_file(entry))
            size += fs::file_size(entry);
    }
    return size;
}

string web_utils::get_action_and_truncate(string &url, bool check_exists) // returns empty string on fail
{
    size_t pos = url.find_last_of("~");
    if (pos != std::string::npos) // check for action
    {
        string action = url.substr(pos + 1);
        url = url.substr(0, pos);
        if (check_exists && !fs::exists(url) && !(url[url.length() - 1] == '/' && fs::exists(url.substr(0, url.length() - 1))))
            return "";
        return action;
    }
    return "";
}

string web_utils::generate_directory_data(string path, std::vector<string> &public_paths, bool cur_public)
{
    string data;
    for (auto &entry : fs::directory_iterator(path))
    {
        string filename = entry.path().filename().string();
        string permission = "public";

        if (!cur_public && check_path_matches(path + filename, public_paths) == MatchType::none)
            permission = "private";

        if (fs::is_directory(entry))
            filename += "/";

        // get_folder_size can be added here to get directory size as well. However, this will make page loading very slow
        // on a low-end device if the directory tree is massive.
        data += encode_webstring(filename) + ";" +
                (fs::is_directory(entry) ? "-" : to_string(fs::file_size(entry))) + ";" +
                permission + "\n";
    }
    auto stat = fs::space("files/");
    data += to_string(stat.capacity) + "\n" + to_string(stat.capacity - stat.free) + "\n";
    data += cur_public ? "public" : "private";
    return data;
}

web_utils::MatchType web_utils::check_path_matches(std::string path, std::vector<std::string> &match_list)
{
    get_action_and_truncate(path, false); // TODO: this will no longer be needed when action processing is finally done properly (with query params)
    if (path[path.length() - 1] == '/')
        path = path.substr(0, path.length() - 1);

    for (auto &m : match_list)
    {
        if (path.rfind(m, 0) == 0)
        {
            if (m.length() == path.length())
                return MatchType::exact;
            if (path[m.length()] == '/')
                return MatchType::prefix;
        }
    }
    return MatchType::none;
}
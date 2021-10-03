#include "web_utils.h"
#include <sstream>

using std::string;
using std::to_string;
using std::unordered_map;

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

string web_utils::human_readable(uint64_t size)
{
    if (size >= 1LL << 30)
    {
        double real_size = double(size) / (1LL << 30);
        int hr_size = int(real_size * 100);
        return to_string(hr_size / 100) + "." + to_string(hr_size % 100) + "GB";
    }
    if (size >= 1 << 20)
        return to_string(size / (1 << 20)) + "MB";
    if (size >= 1 << 10)
        return to_string(size / (1 << 10)) + "KB";
    return to_string(size) + "B";
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
        char c1 = name[pos + 1], c2 = name[pos + 2];
        if (name.size() - pos <= 2 || !strchr(hex_digits, c1) || !strchr(hex_digits, c2) || (c1 == '0' && c2 == '0'))
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

HTTPresponse::MIME web_utils::guess_mime_type(string filepath)
{
    size_t pos = filepath.find_last_of('.');
    string extension = filepath.substr(pos + 1);

    if (extension == "png")
        return HTTPresponse::MIME::png;
    if (extension == "js")
        return HTTPresponse::MIME::javascript;
    if (extension == "html")
        return HTTPresponse::MIME::html;
    if (extension == "css")
        return HTTPresponse::MIME::css;
    // can't guess type, just return byte stream
    return HTTPresponse::MIME::octet_stream;
}
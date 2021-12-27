#include "web_utils.h"
#include "common_utils.h"
#include <sstream>
#include <boost/filesystem.hpp>
#include <set>

namespace fs = boost::filesystem;

using std::set;
using std::string;
using std::to_string;
using std::unordered_map;
using web_utils::add_image;
using web_utils::add_table_image;
using web_utils::human_readable;

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
    if (extension == "mp4")
        return HTTPresponse::MIME::mp4;
    if (extension == "mkv")
        return HTTPresponse::MIME::mkv;
    if (extension == "pdf")
        return HTTPresponse::MIME::pdf;
    // can't guess type, just return byte stream
    return HTTPresponse::MIME::octet_stream;
}

bool web_utils::check_name(string name, const char *not_allowed)
{
    if (name.length() > 255 || name.find("..") != std::string::npos) // don't allow this by default to prevent path injection
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
    size_t pos = url.find("~");
    if (pos != std::string::npos) // check for delete
    {
        string action = url.substr(pos + 1);
        url = url.substr(0, pos);
        if (check_exists && !fs::exists(url) && !(url[url.length() - 1] == '/' && fs::exists(url.substr(0, url.length() - 1))))
            return "";
        return action;
    }
    return "";
}

string web_utils::generate_folder_html(string path, const unsigned int max_name_length)
{
    // TODO: resize PNGs used for this to smaller resolutions
    auto cmp = [](fs::directory_entry a, fs::directory_entry b)
    { return a.path().filename().string() < b.path().filename().string(); };
    set<fs::directory_entry, decltype(cmp)> entries(cmp);
    for (auto &entry : fs::directory_iterator(path))
        entries.insert(entry);
    std::ifstream header("html/table_header.html", std::ios::binary);
    string folder = std::string((std::istreambuf_iterator<char>(header)), std::istreambuf_iterator<char>());
    header.close();
    int i = 0;
    for (auto &file : entries)
    {
        folder += "<tr style=\"background-color:#" + (i % 2 ? string("FFFFFF") : string("808080")) + "\" id=row" + to_string(i) + ">";
        string size, filename = file.path().filename().string(), short_filename = filename.substr(0, max_name_length) + (filename.length() > max_name_length ? " ...  " : "  ");
        constexpr char tdbeg[] = "<td><a href=\"";
        constexpr char tdend[] = "</a></td>";
        string title = "title=\"" + filename + "\">";
        if (!fs::is_directory(file))
        {
            HTTPresponse::MIME type = guess_mime_type(filename);
            size = human_readable(fs::file_size(file));
            folder += add_table_image("file.png");
            folder += tdbeg + filename + "\" " + title + short_filename +
                      (type == HTTPresponse::MIME::mp4 || type == HTTPresponse::MIME::mkv ? "</a><a href=\"" + filename + "/~play/\">" + add_image("play.png") : "") +
                      tdend;
        }
        else
        {
            size = "-";
            folder += add_table_image("folder.png");
            folder += tdbeg + filename + "/\" " + title + short_filename +
                      "</a><button onclick=\"download_check_zip(" + to_string(i) + ")\"/ type=\"button\"" + image_attribute("download.png") +
                      +"></td>";
        }
        folder += "<td>" + size + "</td>";
        folder += "<td><input type=\"checkbox\" name=\"" + filename + "\"></td> ";
        folder += "</tr>";
        i++;
    }
    std::ifstream footer("html/table_footer.html", std::ios::binary);
    folder += std::string((std::istreambuf_iterator<char>(footer)), std::istreambuf_iterator<char>());
    auto stat = fs::space("files/");
    folder += "<p hidden id=free_space>" + to_string(stat.capacity) + "</p><p hidden id=used_space>" + to_string(stat.capacity - stat.free) + "</p>";
    folder += "</body></html>";
    return folder;
}
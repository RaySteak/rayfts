#include "WebServer.h"
#include <algorithm>
#include <set>
#include <spawn.h>
#include <wait.h>
#include <sstream>
#include <boost/filesystem.hpp>

using std::async;
using std::make_pair;
using std::set;
using std::unordered_map;

namespace fs = boost::filesystem;

void WebServer::init_server_params(int port, const char *user, const char *pass)
{
    this->user = user;
    this->pass = pass;
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd < 0, "socket");

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (debug_mode)
    {
        int option = 1;
        ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int));
        DIE(ret < 0, "reuse_addr");
    }
    ret = bind(listenfd, (sockaddr *)&serv_addr, sizeof(sockaddr));
    DIE(ret < 0, "bind");

    ret = listen(listenfd, MAX_CLIENTS);
    DIE(ret < 0, "listen");
    FD_SET(listenfd, &read_fds);
    fdmax = listenfd;

    FD_SET(STDIN_FILENO, &read_fds); // stdin for exit command
}

WebServer::WebServer(int port, const char *user, const char *pass)
{
    init_server_params(port, user, pass);

    // default zip_folder function
    zip_folder = [](char *destination, char *path)
    {
        pid_t pid;
        int status;
        char argv1[] = "/bin/7z", argv2[] = "a";
        char *const argv[] = {argv1, argv2, destination, path, NULL};
        status = posix_spawn(&pid, argv1, NULL, NULL, argv, NULL);
        free(destination), free(path);
        if (status)
            return -1;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
    };
}

WebServer::WebServer(int port, const char *user, const char *pass, std::function<int(char *destination, char *user)> zip_folder)
{
    init_server_params(port, user, pass);

    this->zip_folder = zip_folder;
}

int WebServer::send_exactly(int fd, const char *buffer, size_t count)
{
    int nr = 0;
    while (count)
    {
        int n = send(fd, buffer + nr, count, 0);
        if (n < 0)
            return n;
        nr += n;
        count -= n;
    }
    return nr;
}

int WebServer::recv_exactly(int fd, char *buffer, size_t count, timeval t) //TODO: implement timeval
{
    int nr = 0;
    while (count)
    {
        int n = recv(fd, buffer + nr, count, 0);
        if (n <= 0)
            return n;
        nr += n;
        count -= n;
    }
    return nr;
}

WebServer::Method get_method(char *data)
{
    if (!strncmp(data, "GET", 3))
        return WebServer::Method::GET;
    if (!strncmp(data, "HEAD", 4))
        return WebServer::Method::HEAD;
    if (!strncmp(data, "POST", 4))
        return WebServer::Method::POST;
    if (!strncmp(data, "PUT", 3))
        return WebServer::Method::PUT;
    if (!strncmp(data, "DELETE", 6))
        return WebServer::Method::DELETE;
    if (!strncmp(data, "OPTIONS", 7))
        return WebServer::Method::OPTIONS;
    return WebServer::Method::nil;
}

int WebServer::recv_http_header(int fd, char *buffer, int max, int &header_size)
{
    int nr = 0;
    char *terminator = NULL;

    while (nr < max)
    {
        pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
        ret = poll(&pfd, 1, timeout_milli);
        DIE(ret < 0, "poll");
        if (!(pfd.revents & POLLIN))
            return 0;
        int n = recv(fd, buffer + nr, max - nr, 0);
        if (n <= 0)
            return n;
        buffer[nr + n] = 0;
        if (nr == 0)
        {
            if (get_method(buffer) == Method::nil)
                return 0;
            char *http = strstr(buffer, "HTTP/1.1");
            if (!http)
                return 0;
            char *first_crlf = strstr(buffer, CRLF);
            if (!first_crlf || first_crlf - http < 0)
                return 0;
            char *url = strchr(buffer, '/');
            if (!url || url - http > 0)
                return 0;
        }
        nr += n;
        if ((terminator = strstr(buffer, CRLFx2)))
            break;
    }

    if (!terminator)
        return 0;
    header_size = terminator - buffer + strlen(CRLFx2);
    return nr;
}

void WebServer::close_connection(int fd, bool erase_from_sets)
{
    if (erase_from_sets)
    {
        auto found_it = file_futures.find(fd);
        if (found_it != file_futures.end())
        {
            fs::remove(found_it->second.second.get_filename());
            file_futures.erase(found_it);
        }
        unsent_files.erase(fd);
        unreceived_files.erase(fd);
    }

    ret = close(fd);
    FD_CLR(fd, &read_fds);

    for (int i = fdmax; i > 0; i--)
    {
        if (FD_ISSET(i, &read_fds))
        {
            fdmax = i;
            break;
        }
    }
}

int WebServer::process_http_header(int fd, char *buffer, int read_size, int header_size, char *&data, uint64_t &total)
{
    size_t nr = read_size;
    data = NULL;
    char *content_length = strstr(buffer, "Content-Length: ");

    if (content_length)
    {
        content_length += strlen("Content-Length: ");
        //TODO: check big content lengths
        int digits = 0;
        while (content_length[digits] >= '0' && content_length[digits] <= '9')
            digits++;
        content_length[digits] = 0;
        uint64_t length = strtoull(content_length, NULL, 0);
        content_length[digits] = '\r';
        uint64_t content_read = read_size - header_size;
        uint64_t remaining = length - content_read;
        total = remaining + read_size;
        if (total > max_alloc)
            data = new char[max_alloc + 1];
        else
            data = new char[total + 1];
        memcpy(data, buffer, read_size);

        while (remaining && max_alloc - nr > 0)
        {
            pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
            ret = poll(&pfd, 1, timeout_milli);
            DIE(ret < 0, "poll");
            if (!(pfd.revents & POLLIN))
            {
                delete data;
                return 0;
            }
            int n = recv(fd, data + nr, total > max_alloc ? max_alloc - nr : remaining, 0);
            if (n <= 0)
            {
                delete data;
                return n;
            }
            nr += n;
            remaining -= n;
        }
    }
    else
    {
        data = new char[header_size + 1];
        total = header_size;
        memcpy(data, buffer, header_size);
    }
    data[nr] = 0;
    return nr;
}

unordered_map<string, string> get_content_fields(char *content, const char *is, const char *separators)
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

void WebServer::process_cookies()
{
    for (auto it = cookies.begin(); it != cookies.end();)
    {
        Cookie *cookie = it->second;

        cookie->update();
        if (cookie->is_expired())
        {
            delete cookie;
            it = cookies.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void WebServer::remove_from_read(int fd)
{
    FD_CLR(fd, &read_fds);
    for (int i = fdmax; i > 0; i--)
    {
        if (FD_ISSET(i, &read_fds))
        {
            fdmax = i;
            break;
        }
    }
}
void WebServer::add_to_read(int fd)
{
    FD_SET(fd, &read_fds);
    fdmax = fd > fdmax ? fd : fdmax;
}

string human_readable(uint64_t size)
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

inline string add_image(string image)
{
    return "<img src=\"/~images/" + image + "\" height=\"20\" width=\"20\">";
}

inline string add_table_image(string image)
{
    return "<td>" + add_image(image) + "</td>";
}

string generate_folder_html(string path)
{
    //TODO: resize PNGs used for this to smaller resolutions
    const int max_name_length = 30;
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
        folder += "<tr style=\"background-color:#" + (i % 2 ? string("FFFFFF") : string("808080")) + "\">";
        string size, filename = file.path().filename().string(), short_filename = filename.substr(0, max_name_length) + (filename.length() > max_name_length ? " ...  " : "  ");
        constexpr char tdbeg[] = "<td><a href=\"";
        constexpr char tdend[] = "</a></td>";
        string title = "title=\"" + filename + "\">";
        if (!fs::is_directory(file))
        {
            size = human_readable(fs::file_size(file));
            folder += add_table_image("file.png");
            folder += tdbeg + filename + "\"" + title + short_filename + tdend;
        }
        else
        {
            size = "-";
            folder += add_table_image("folder.png");
            folder += tdbeg + filename + "/\"" + title + short_filename +
                      "</a><a href=\"" + filename + "/~archive\">" + add_image("download.png") + tdend;
        }
        folder += "<td>" + size + "</td>";
        folder += "<td><input type=\"checkbox\" name=\"" + filename + "\"></td> ";
        folder += "</tr>";
        i++;
    }
    std::ifstream footer("html/table_footer.html", std::ios::binary);
    folder += std::string((std::istreambuf_iterator<char>(footer)), std::istreambuf_iterator<char>());
    return folder;
}

string parse_webstring(string name)
{
    const char hex_digits[] = "0123456789ABCDEF";
    size_t pos = 0;
    while ((pos = name.find('+', pos)) != string::npos)
        name[pos] = ' ';
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

bool check_name(string name)
{
    if (name.length() > 255 || name.find("..") != std::string::npos)
        return false;
    for (auto &c : name)
    {
        if (strchr("&/%~=", c)) // list of characters not accepted
            return false;
    }
    return true;
}

string get_action_and_truncate(string &url) // returns empty string on fail
{
    size_t pos = url.find("~");
    if (pos != std::string::npos) //check for delete
    {
        string action = url.substr(pos + 1);
        url = url.substr(0, pos);
        std::ifstream check_exists("files" + url, std::ios::binary);
        if (!check_exists)
            return "";
        return action;
    }
    return "";
}

HTTPresponse WebServer::process_http_request(char *data, int header_size, size_t read_size, uint64_t total_size, int fd)
{
    char *content = data + header_size;
    auto not_implemented = HTTPresponse(501).file_attachment("html/not_implemented.html", HTTPresponse::MIME::html);
    auto not_found = HTTPresponse(404).file_attachment("html/not_found.html", HTTPresponse::MIME::html);
    const string redirect = "Redirecting...";

    Method method = get_method(data);
    char *url = strchr(data, '/');
    char *saved;
    strtok_r(url, " \r\n", &saved);
    string url_string = parse_webstring(string(url));
    data[header_size - 1] = 0;
    strtok_r(NULL, "\r\n", &saved);
    auto http_fields = get_content_fields(saved, ": ", CRLF);

    if (strstr(url, ".."))
        return HTTPresponse(401)
            .file_attachment(string("Incercati sa ma hackati dar in balta va-necati"), HTTPresponse::MIME::text);
    if (!strcmp(url, "/login"))
    {
        switch (method)
        {
        case Method::GET:
            return HTTPresponse(200).file_attachment("html/login.html", HTTPresponse::MIME::html);
        case Method::POST:
        {
            auto fields = get_content_fields(content, "=", "&");
            //TODO: verifications for empty fields or no user or pass fields.
            //TODO: implement remember after cookies are implemented
            string username, password, remember;
            username = fields["user"];
            password = fields["psw"];
            remember = fields["remember"];
            if (username == user && password == pass)
            {
                //TODO: maybe check if an ip already generated a cookie
                SessionCookie *cookie = new SessionCookie();
                cookies.insert({cookie->val(), cookie});

                return HTTPresponse(302).cookie(cookie).location("/").file_attachment(redirect, HTTPresponse::MIME::text);
            }
            return HTTPresponse(401).file_attachment("html/login_error.html", HTTPresponse::MIME::html);
        }
        default:
            return not_implemented;
        }
    }
    else if (!strncmp(url, "/~images/", 9))
    {
        fs::directory_entry check(url + 1);
        if (!fs::exists(check) || fs::is_directory(check))
            return not_found;
        return HTTPresponse(200).file_attachment(url + 1, HTTPresponse::MIME::png);
    }
    else if (!strcmp(url, "/favicon.ico"))
    {
        return HTTPresponse(200).file_attachment("ico/favicon.ico", HTTPresponse::MIME::icon);
    }
    else if (!strncmp(url, "/.well-known/pki-validation/", 28))
    {
        return HTTPresponse(200).file_attachment(url + 1, HTTPresponse::MIME::text);
    }
    else
    {
        //TODO: redirect something like "/test" to "/test/" for directory queries
        HTTPresponse login_page = HTTPresponse(302).location("/login").file_attachment(redirect, HTTPresponse::MIME::text);
        string cookie = http_fields["Cookie"];
        if (cookie == "") // no cookie
            return login_page;
        //TODO: also check the other cookies
        char *cookie_copy = strdup(cookie.c_str());
        auto cookie_map = get_content_fields(cookie_copy, "=", ";");
        delete cookie_copy;
        bool found_login_cookie = false;
        for (auto &[name, val] : cookie_map)
        {
            if (name == "sessionId" && cookies.find(val) != cookies.end())
                found_login_cookie = true;
        }
        if (!found_login_cookie)
            return login_page;

        string path = "files" + url_string;
        fs::directory_entry dir;
        try
        {
            dir = fs::directory_entry(path);
        }
        catch (const std::exception &e)
        {
            return HTTPresponse(422).file_attachment(string("422 Unprocessable entity"), HTTPresponse::MIME::text);
        }
        std::ifstream file(path, std::ios::binary);
        bool doesnt_exist = !fs::exists(dir); // file opens for both folders and files
        if (!doesnt_exist)
        {
            if (fs::is_directory(dir) && url_string[url_string.length() - 1] != '/')
                return HTTPresponse(303).location(url_string + "/").file_attachment(redirect, HTTPresponse::MIME::text);
        }

        switch (method)
        {
        case Method::GET:
            if (doesnt_exist)
            {
                string action = get_action_and_truncate(url_string);
                if (action == "archive")
                {
                    //TODO: find an elegant way to kill any process created by zip_folder ran in async, as they still run when connection is closed
                    //what's weird is that the file gets removed succesfully but /bin/7z doesn't seem to notice it's writing into the void??
                    size_t delim = url_string.find_last_of('/', url_string.length() - 2);
                    string folder_name = url_string.substr(delim + 1, url_string.size() - delim - 2);
                    string filename = "temp/TEMP", path = "./files" + url_string.substr(0, url_string.size() - 1);
                    if (!fs::exists(fs::directory_entry("temp")))
                        fs::create_directory("temp");
                    auto it = fs::directory_iterator("temp/");
                    int nr = std::count_if(fs::begin(it), fs::end(it), [](fs::directory_entry e)
                                           { return fs::is_regular_file(e); });
                    filename = filename + to_string(nr + 1) + ".zip";
                    //TODO: with current code, filename gets copied twice, solve that
                    auto response = HTTPresponse(200).content_disposition(HTTPresponse::DISP::Attachment, folder_name + ".zip").file_promise(filename.c_str());
                    file_futures.insert({fd, make_pair(async(std::launch::async, zip_folder, strdup(filename.c_str()), strdup(path.c_str())), response)});
                    return response;
                }
                return not_found;
            }
            if (!fs::is_directory(dir))
                return HTTPresponse(200).file_attachment(path.c_str(), HTTPresponse::MIME::octet_stream);
            return HTTPresponse(200).file_attachment(generate_folder_html(path), HTTPresponse::MIME::html);
        case Method::POST:
        {
            if (doesnt_exist)
            {
                //TODO: check if path to /~delete exists
                string action = get_action_and_truncate(url_string);
                if (action == "delete")
                {
                    auto fields = get_content_fields(content, "=", "&");
                    for (auto &field : fields)
                        fs::remove_all("files" + url_string + parse_webstring(field.first));
                    return HTTPresponse(303).location(url_string).file_attachment(redirect, HTTPresponse::MIME::text);
                }
                return not_found;
            }
            if (!fs::is_directory(dir))
                return not_implemented;

            string content_type = http_fields["Content-Type"];
            if (content_type != "")
            {
                size_t semicolon = content_type.find("; ");
                string type = content_type.substr(0, semicolon);
                if (type == "multipart/form-data") // file upload
                {
                    string boundary = content_type.substr(semicolon + 2, content_type.length() - semicolon - 2);
                    const int id_len = strlen("boundary=");
                    if (!boundary.compare(0, id_len, "boundary="))
                    {
                        string boundary_val = boundary.substr(id_len, boundary.length() - id_len);
                        if (!strncmp(content, ("--" + boundary_val).c_str(), boundary_val.length()))
                        {
                            content += boundary_val.length() + strlen(CRLF);
                            char *file_content = strstr(content, CRLFx2);
                            if (file_content)
                            {
                                file_content[0] = 0;
                                file_content += strlen(CRLFx2);
                                auto file_fields = get_content_fields(content, ": ", CRLF);
                                string content_disposition = file_fields["Content-Disposition"];
                                if (content_disposition != "")
                                {
                                    size_t filename_pos = content_disposition.find("filename=");
                                    if (filename_pos != string::npos)
                                    {
                                        filename_pos += strlen("filename=") + 1;
                                        size_t filename_end = content_disposition.find('\"', filename_pos);
                                        if (filename_end != string::npos)
                                        {
                                            string filename = content_disposition.substr(filename_pos, filename_end - filename_pos);
                                            if (!check_name(filename))
                                                return HTTPresponse(422).location(url_string).file_attachment(redirect, HTTPresponse::MIME::text);
                                            size_t file_header_size = file_content - content + boundary_val.length() + strlen(CRLF);
                                            // clear fd so that we treat file reception separately (NOTE: this doesn't close the connection)
                                            remove_from_read(fd);
                                            unreceived_files.insert(
                                                {fd, file_receive_data("files" + url_string + filename, boundary_val, file_content, read_size - file_header_size - header_size, total_size - read_size)});
                                            return HTTPresponse(HTTPresponse::PHONY);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return HTTPresponse(400).end_header();
                }
            }
            auto fields = get_content_fields(content, "=", "&");
            //TODO: also check here (both existence of folder_name and also succesful directory creation)
            string folder_name = parse_webstring(fields["folder_name"]);
            // This check is redundant if user operates from browser, because it is also performed
            // in the javascript of the site but requests might not come from browsers
            if (!check_name(folder_name))
                return HTTPresponse(303).location(url_string).file_attachment(redirect, HTTPresponse::MIME::text);
            fs::create_directory(path + folder_name);
            //responsd with redirect (Post/Redirect/Get pattern) to avoid form resubmission
            return HTTPresponse(303).location(url_string).file_attachment(redirect, HTTPresponse::MIME::text);
        }
        default:
            return not_implemented;
        }
    }
}

void WebServer::run()
{
    while (1)
    {
        tmp_fds = read_fds;
        int available_requests;
        if (unsent_files.size() == 0 && unreceived_files.size() == 0 && file_futures.size() == 0)
        {
            available_requests = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        }
        else
        {
            timeval zero_time{.tv_sec = 0, .tv_usec = 0};
            available_requests = select(fdmax + 1, &tmp_fds, NULL, NULL, &zero_time);
        }
        DIE(available_requests < 0, "select");

        // check available future files
        for (auto map_iterator = file_futures.begin(); map_iterator != file_futures.end();)
        {
            if (map_iterator->second.first.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                // TODO: use return value of future and also the other double filename copy thingy
                // maybe create a method for when the filename is already set, called attach_file
                // TODO: is it worth it sending part of response immediately and the rest later?
                auto &response = map_iterator->second.second;
                response.file_attachment(response.get_filename().c_str(), HTTPresponse::MIME::zip, [](const char *filename)
                                         { fs::remove(filename); });
                send_exactly(map_iterator->first, response.to_c_str(), response.size());
                unsent_files.insert({map_iterator->first, response.begin_file_transfer()});
                map_iterator = file_futures.erase(map_iterator);
                continue;
            }
            map_iterator++;
        }
        // send files
        for (auto map_iterator = unsent_files.begin(); map_iterator != unsent_files.end();)
        {
            pollfd pfd{.fd = map_iterator->first, .events = POLLOUT, .revents = 0};
            ret = poll(&pfd, 1, 0);
            DIE(ret < 0, "poll");
            if (!(pfd.revents & POLLOUT))
            {
                map_iterator++;
                continue;
            }
            n = send_exactly(map_iterator->first, map_iterator->second->fragment, map_iterator->second->size);
            if (n < 0)
            {
                close_connection(map_iterator->first, false);
                map_iterator = unsent_files.erase(map_iterator);
            }
            else
            {
                map_iterator->second++;
                if (map_iterator->second.has_next())
                    map_iterator++;
                else
                    map_iterator = unsent_files.erase(map_iterator);
            }
        }
        // receive files
        for (auto map_iterator = unreceived_files.begin(); map_iterator != unreceived_files.end();)
        {
            auto &f = map_iterator->second;
            //TODO: build mechanism to check cancel of receive and also timeout
            f.file->write(f.data[f.current_buffer], f.last_recv);
            if (!f.remaining)
            {
                add_to_read(map_iterator->first);
                char *current_buffer = f.data[f.current_buffer];
                char *prev_buffer = f.get_next_buffer();
                uint64_t size = f.file->tellp(), bs = f.boundary.size();
                char temp_buffer[2 * max_alloc];
                memcpy(temp_buffer + (max_alloc - f.prev_recv), prev_buffer, f.prev_recv);
                memcpy(temp_buffer + max_alloc, current_buffer, f.last_recv);
                size_t offset = bs + strlen("--") + strlen(CRLF);
                if (strncmp(temp_buffer + max_alloc + f.last_recv - offset, f.boundary.c_str(), bs))
                {
                    fs::remove_all(f.filename);
                }
                else
                {
                    fs::resize_file(f.filename, size - offset - strlen("--") - strlen(CRLF));
                    //TODO: return upload time here maybe and process it in ajax
                    HTTPresponse confirmation = HTTPresponse(200).file_attachment(string("success"), HTTPresponse::MIME::text);
                    send_exactly(map_iterator->first, confirmation.to_c_str(), confirmation.size());
                }
                map_iterator = unreceived_files.erase(map_iterator);
                continue;
            }

            n = recv_exactly(map_iterator->first, f.get_next_buffer(), f.remaining > max_alloc ? max_alloc : f.remaining, {0, 0}); //TODO: also change timeval here
            if (n <= 0)
            {
                close_connection(map_iterator->first, false);
                fs::remove_all(f.filename);
                map_iterator = unreceived_files.erase(map_iterator);
                continue;
            }

            f.prev_recv = f.last_recv;
            f.last_recv = n;
            f.remaining -= n;
            map_iterator++;
        }

        if (available_requests != 0)
        {
            process_cookies();
            for (i = 0; i <= fdmax; i++)
            {
                if (FD_ISSET(i, &tmp_fds))
                {
                    if (i == STDIN_FILENO)
                    {
                        if (!fgets(buffer, BUFLEN, stdin))
                            break;
                        if (!strncmp(buffer, "exit", 4))
                            break;
                        fprintf(stderr, "Command not recognized!\n");
                    }
                    else if (i == listenfd)
                    {
                        newsockfd = accept(listenfd, (sockaddr *)&cli_addr, &socklen);
                        DIE(newsockfd < 0, "accept");
                        /*int option = 1;
                        ret = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(int));
                        DIE(ret < 0, "no_delay");*/
                        FD_SET(newsockfd, &read_fds);
                        fdmax = newsockfd > fdmax ? newsockfd : fdmax;
                    }
                    else
                    {
                        int header_size;
                        n = recv_http_header(i, buffer, BUFLEN, header_size);
                        if (n <= 0) // connection closed or incorrect request or timeout
                        {
                            close_connection(i, true);
                            continue;
                        }

                        char *data;
                        uint64_t total;
                        n = process_http_header(i, buffer, n, header_size, data, total);
                        DIE(n < 0, "process");
                        if (n == 0)
                        {
                            close_connection(i, true);
                            continue;
                        }

                        std::cout << data << '\n';
                        std::cout << "Pregatim raspunsul...\n";
                        HTTPresponse response = process_http_request(data, header_size, n, total, i);
                        std::cout << "Trimitem raspunsul\n";
                        //std::cout << response;
                        if (response.is_phony())
                            continue;
                        if (!response.is_promise_transfer())
                        {
                            send_exactly(i, response.to_c_str(), response.size());
                            if (response.is_multifragment_transfer())
                                unsent_files.insert({i, response.begin_file_transfer()});
                        }
                        std::cout << "Am trimis raspunsul\n";

                        delete data;
                    }
                }
            }
            if (i <= fdmax)
                break;
        }
    }
}

WebServer::~WebServer()
{
    for (int i = 0; i <= fdmax; i++)
    {
        if (FD_ISSET(i, &read_fds))
        {
            ret = close(i);
            DIE(ret < 0, "close");
        }
    }
    for (auto &[id, c] : cookies)
        delete c;
}
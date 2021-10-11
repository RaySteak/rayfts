#include "WebServer.h"
#include "web_utils.h"
#include "wake_on_lan.h"
#include "remote_shutdown/remote_shutdown.h"
#include <algorithm>
#include <set>
#include <spawn.h>
#include <wait.h>
#include <boost/filesystem.hpp>
#include <sstream>

#define IF_IS

using namespace web_utils;
using std::async;
using std::make_pair;
using std::make_tuple;
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

    // create folders ./temp and ./files
    const char *const required_folders[] = {"files", "temp", NULL};
    for (int i = 0; required_folders[i]; i++)
    {
        if (!fs::exists(required_folders[i]))
        {
            fs::create_directory(required_folders[i]);
        }
        else if (!fs::is_directory(required_folders[i]))
        {
            std::cerr << "Server cannot run because there's already a file named \"" << required_folders[i] << "\"\n";
            std::cerr << "Please remove said file for the server to run\n";
            exit(1);
        }
    }

    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpfd < 0, "socket");
}

WebServer::WebServer(int port, const char *user, const char *pass)
{
    init_server_params(port, user, pass);

    // default zip_folder function
    zip_folder = [](char *destination, char *path, WebServer *server)
    {
        pid_t pid;
        int status;
        char argv0[] = "/bin/7z", argv1[] = "a", argv4[] = "-mx0";
        char *const argv[] = {argv0, argv1, destination, path, argv4, NULL};
        status = posix_spawn(&pid, argv0, NULL, NULL, argv, NULL);
        server->path_to_pid.insert({string(path + strlen("./")), pid});
        free(destination), free(path);
        if (status)
            return -1;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
    };
}

WebServer::WebServer(int port, const char *user, const char *pass, std::function<int(char *destination, char *user, WebServer *)> zip_folder)
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
    if (!startcmp(data, "GET"))
        return WebServer::Method::GET;
    if (!startcmp(data, "HEAD"))
        return WebServer::Method::HEAD;
    if (!startcmp(data, "POST"))
        return WebServer::Method::POST;
    if (!startcmp(data, "PUT"))
        return WebServer::Method::PUT;
    if (!startcmp(data, "DELETE"))
        return WebServer::Method::DELETE;
    if (!startcmp(data, "OPTIONS"))
        return WebServer::Method::OPTIONS;
    if (!startcmp(data, "PATCH"))
        return WebServer::Method::PATCH;
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
    auto found_it = fd_to_file_futures.find(fd);
    if (found_it != fd_to_file_futures.end())
    {
        auto path = temp_to_path[found_it->second];
        auto future_it = file_futures.find(path);
        if (future_it != file_futures.end())
        {
            auto &fd_set = std::get<0>(future_it->second);
            fd_set.erase(fd);
            if (fd_set.empty())
            {
                temp_to_path.erase(found_it->second);
                fs::remove(found_it->second); // remove file
                auto pid_it = path_to_pid.find(path);
                if (pid_it != path_to_pid.end())
                {
                    kill(pid_it->second, SIGKILL);
                    path_to_pid.erase(path);
                }
                file_futures.erase(future_it);
            }
        }
        fd_to_file_futures.erase(fd);
    }

    if (erase_from_sets)
    {
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

HTTPresponse WebServer::queue_file_future(int fd, string temp_path, string folder_path, string folder_name)
{
    auto file_future_it = fd_to_file_futures.find(fd);
    if (file_future_it != fd_to_file_futures.end())
    {
        if (file_futures.find(temp_to_path[file_future_it->second]) != file_futures.end())
            return HTTPresponse(503).file_attachment(string("You can't queue zipping again!"), HTTPresponse::MIME::text);
    }
    auto future_it = file_futures.find(folder_path);
    auto downloading_it = downloading_futures.find(folder_path);
    if (future_it == file_futures.end() && downloading_it == downloading_futures.end()) // nobody neither waiting for creation nor for receiving
    {
        fd_to_file_futures.insert({fd, temp_path});
        temp_to_path.insert({temp_path, folder_path});
        auto response = HTTPresponse(200).content_disposition(HTTPresponse::DISP::Attachment, folder_name + ".zip").file_promise(temp_path.c_str());
        unordered_set<int> new_set;
        new_set.insert(fd);
        string full_path = "./" + folder_path;
        file_futures.insert(
            {folder_path, make_tuple(new_set, async(std::launch::async, zip_folder, strdup(temp_path.c_str()), strdup(full_path.c_str()), this), response, temp_path, get_folder_size(full_path))});
        return response;
    }
    if (future_it != file_futures.end()) // file is still being created, return the incomplete response
    {
        fd_to_file_futures.insert({fd, std::get<3>(future_it->second)});
        std::get<0>(future_it->second).insert(fd);
        return std::get<2>(future_it->second);
    }
    // file is still being downloaded by others, begin transfer directly
    downloading_it->second.first++;
    return downloading_it->second.second;
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
    string url_string = parse_webstring(string(url), false);
    data[header_size - 1] = 0;
    strtok_r(NULL, "\r\n", &saved);
    auto http_fields = get_content_fields(saved, ": ", CRLF);

    if (strstr(url, ".."))
        return HTTPresponse(401).file_attachment(string("Incercati sa ma hackati dar in balta va-necati"), HTTPresponse::MIME::text);
    else if (!strcmp(url, "/login"))
    {
        switch (method)
        {
        case Method::GET:
            return HTTPresponse(200).file_attachment("html/login.html", HTTPresponse::MIME::html);
        case Method::POST:
        {
            auto fields = get_content_fields(content, "=", "&");
            //TODO: implement remember
            string username, password, remember;
            username = fields["user"];
            password = fields["psw"];
            remember = fields["remember"];
            if (username == user && password == pass)
            {
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
    else if (!startcmp(url, "/images/") || !startcmp(url, "/js/") || !startcmp(url, "/css/") || !startcmp(url, "/.well-known/pki-validation/")) // public folders
    {
        switch (method)
        {
        case Method::OPTIONS:
            return HTTPresponse(200).access_control("*").file_attachment(string("Te las boss sa te uiti numa"), HTTPresponse::MIME::text);
        case Method::GET:
        case Method::POST:
        {
            fs::directory_entry check(url + 1);
            if (!fs::exists(check) || fs::is_directory(check))
                return not_found;
            return HTTPresponse(200).access_control("*").file_attachment(url + 1, guess_mime_type(url + 1));
        }
        default:
            return not_implemented;
        }
    }
    else if (!startcmp(url, "/files/") || !strcmp(url, "/") || !startcmp(url, "/control")) // private stuff that requires passwords
    {
        HTTPresponse login_page = HTTPresponse(302).location("/login").file_attachment(redirect, HTTPresponse::MIME::text);
        string cookie = http_fields["Cookie"];
        char *cookie_copy = strdup(cookie.c_str());
        auto cookie_map = get_content_fields(cookie_copy, "=", ";");
        free(cookie_copy);
        bool found_login_cookie = false;
        for (auto &[name, val] : cookie_map)
        {
            auto found_cookie = cookies.find(val);
            if (name == "sessionId" && found_cookie != cookies.end())
            {
                found_cookie->second->renew();
                found_login_cookie = true;
                break;
            }
        }
        if (!debug_mode && !found_login_cookie)
            return login_page;

        url_string = url_string.substr(1);
        string path = url_string;
        fs::directory_entry dir;
        try
        {
            dir = fs::directory_entry(path);
        }
        catch (const std::exception &e)
        {
            return HTTPresponse(422).file_attachment(string("422 Unprocessable entity"), HTTPresponse::MIME::text);
        }
        //std::ifstream file(path, std::ios::binary);
        bool doesnt_exist = !fs::exists(dir);
        if (!doesnt_exist)
        {
            if (fs::is_directory(dir) && url_string[url_string.length() - 1] != '/')
                return HTTPresponse(303).location("/" + url_string + "/").file_attachment(redirect, HTTPresponse::MIME::text);
        }
        else
        {
            if (url_string[url_string.length() - 1] == '/' && fs::exists(url_string.substr(0, url_string.length() - 1)))
                return HTTPresponse(303).location("/" + url_string.substr(0, url_string.length() - 1)).file_attachment(redirect, HTTPresponse::MIME::text);
        }

        switch (method)
        {
        case Method::GET:
            if (!strcmp(url, "/"))
                return HTTPresponse(200).file_attachment("html/select.html", HTTPresponse::MIME::html);
            if (!startcmp(url, "/control"))
            {
                if (!strcmp(url, "/control"))
                {
                    auto wol_list = wol::wake_on_lan::parse_list("WolList.txt");
                    std::ifstream control("html/control.html", std::ios::binary);
                    string response = std::string((std::istreambuf_iterator<char>(control)), std::istreambuf_iterator<char>());
                    response += "<script>";
                    for (auto &w : wol_list)
                        response += "add_device(\"" + w.get_device_name() + " (" + w.get_ip() + ")\",\"" + w.get_mac_readable() + "\");";
                    response += "</script></body></html>";
                    return HTTPresponse(200).file_attachment(response, HTTPresponse::MIME::html);
                }
                auto action = get_action_and_truncate(url_string, false);
                if (action == "up")
                {
                    string on_states;
                    auto wol_list = wol::wake_on_lan::parse_list("WolList.txt");
                    for (auto &w : wol_list)
                    {
                        // TODO: add a future style approach instead
                        const unsigned int wait_ms = 10;
                        pid_t pid;
                        char argv0[] = "/bin/fping", argv1[] = "-c1", argv2[15] = "-t", *argv3 = strdup(w.get_ip().c_str());
                        strcat(argv2, std::to_string(wait_ms).c_str());
                        char *const argv[] = {argv0, argv1, argv2, argv3, NULL};
                        int status = posix_spawn(&pid, argv0, NULL, NULL, argv, NULL);
                        free(argv3);
                        if (status)
                        {
                            std::cerr << "/bin/fping not found, please install it\n";
                            return HTTPresponse(404).file_attachment(string("Please install fping on the server\n"), HTTPresponse::MIME::text);
                        }
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                            on_states += '1';
                        else
                            on_states += '0';
                    }
                    return HTTPresponse(200).file_attachment(on_states, HTTPresponse::MIME::text);
                }
            }
            if (doesnt_exist)
            {
                string action = get_action_and_truncate(url_string);
                if (action == "archive")
                {
                    size_t delim = url_string.find_last_of('/', url_string.length() - 2);
                    string folder_name = url_string.substr(delim + 1, url_string.size() - delim - 2);
                    string filename = "temp/TEMP", path = url_string.substr(0, url_string.size() - 1);
                    auto it = fs::directory_iterator("temp/");
                    int nr = std::count_if(fs::begin(it), fs::end(it), [](fs::directory_entry e)
                                           { return fs::is_regular_file(e); });
                    filename = filename + to_string(nr + 1) + ".zip";
                    return queue_file_future(fd, filename, path, folder_name);
                }
                if (action == "check")
                {
                    auto found_future = file_futures.find(url_string.substr(0, url_string.length() - 1));
                    if (found_future != file_futures.end())
                    {
                        uint64_t temp_size;
                        try
                        {
                            temp_size = fs::file_size(std::get<3>(found_future->second));
                        }
                        catch (const std::exception &e)
                        {
                            return not_found;
                        }
                        uint64_t full_size = std::get<4>(found_future->second);
                        return HTTPresponse(200).file_attachment(to_string(temp_size) + ' ' + to_string(full_size), HTTPresponse::MIME::text);
                    }
                }
                if (action == "play/") // used as a folder to simplify html
                {
                    return HTTPresponse(200).file_attachment("html/video.html", HTTPresponse::MIME::html);
                }
                return not_found;
            }
            if (!fs::is_directory(dir))
            {
                uint64_t begin_offset = 0;
                if (http_fields["Range"] != "") // check if browser wants a start range
                {
                    char *copy = strdup(http_fields["Range"].c_str());
                    auto range_fields = get_content_fields(copy, "=", ";");
                    std::stringstream nr(range_fields["bytes"]);
                    nr >> begin_offset;
                    free(copy);
                    return HTTPresponse(206).file_promise(path.c_str()).content_range(begin_offset).attach_file(HTTPresponse::MIME::octet_stream);
                }
                else
                {
                    return HTTPresponse(200).file_attachment(path.c_str(), HTTPresponse::MIME::octet_stream);
                }
            }
            return HTTPresponse(200).file_attachment(generate_folder_html(path), HTTPresponse::MIME::html);
        case Method::POST:
        {
            if (doesnt_exist)
            {
                string action = get_action_and_truncate(url_string);
                if (action == "delete")
                {
                    auto fields = get_content_fields(content, "=", "&");
                    for (auto &field : fields)
                        fs::remove_all(url_string + parse_webstring(field.first, true));
                    return HTTPresponse(303).location("/" + url_string).file_attachment(redirect, HTTPresponse::MIME::text);
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
                                                return HTTPresponse(422).location("/" + url_string).file_attachment(redirect, HTTPresponse::MIME::text);
                                            size_t file_header_size = file_content - content + boundary_val.length() + strlen(CRLF);
                                            // clear fd so that we treat file reception separately (NOTE: this doesn't close the connection)
                                            remove_from_read(fd);
                                            unreceived_files.insert(
                                                {fd, file_receive_data(url_string + filename, boundary_val, file_content, read_size - file_header_size - header_size, total_size - read_size)});
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
            string folder_name = parse_webstring(fields["folder_name"], true);
            // This check is redundant if user operates from browser, because it is also performed
            // in the javascript of the site but requests might not come from browsers
            if (!check_name(folder_name))
                return HTTPresponse(303).location("/" + url_string).file_attachment(redirect, HTTPresponse::MIME::text);
            fs::create_directory(path + folder_name);
            //responsd with redirect (Post/Redirect/Get pattern) to avoid form resubmission
            return HTTPresponse(303).location("/" + url_string).file_attachment(redirect, HTTPresponse::MIME::text);
        }
        case Method::PATCH:
        {
            auto fields = get_content_fields(content, "=", "&");
            if (!strcmp(url, "/control/~awaken"))
            {
                if (fields["turn_on"] == "")
                    return HTTPresponse(400).end_header();
                wol::wake_on_lan(fields["turn_on"]).awaken();
                return HTTPresponse(200).file_attachment(string("Awaken command sent"), HTTPresponse::MIME::text);
            }
            if (!strcmp(url, "/control/~sleep"))
            {
                if (fields["ip"] == "")
                    return HTTPresponse(400).end_header();
                cli_addr.sin_family = AF_INET;
                cli_addr.sin_port = htons(REMOTE_SHUTDOWN_PORT);
                inet_aton(fields["ip"].c_str(), &cli_addr.sin_addr);
                sendto(udpfd, REMOTE_SHUTDOWN_KEYWORD, strlen(REMOTE_SHUTDOWN_KEYWORD), 0, (sockaddr *)&cli_addr, sizeof(cli_addr));
                return HTTPresponse(200).file_attachment(string("Sleep command sent"), HTTPresponse::MIME::text);
            }
            return not_found;
        }
        break;
        default:
            return not_implemented;
        }
    }
    return not_found;
}

void WebServer::run()
{
    while (1)
    {
        //std::cout << file_futures.size() << ' ' << downloading_futures.size() << ' ' << temp_to_path.size() << ' ' << fd_to_file_futures.size() << ' ' << path_to_pid.size() << '\n';
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
            auto &fds = std::get<0>(map_iterator->second);
            auto &future = std::get<1>(map_iterator->second);
            auto &response = std::get<2>(map_iterator->second);
            if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                // TODO: use return value of future
                response.attach_file(
                    HTTPresponse::MIME::zip, [](const char *filename, void *action_object)
                    {
                        WebServer *server = (WebServer *)action_object;
                        string folder_path = server->temp_to_path[filename];
                        auto entry = server->downloading_futures.find(folder_path);
                        if (entry->second.first == 1) // only this connection is downloading this file
                        {
                            fs::remove(filename);
                            server->temp_to_path.erase(filename);
                            server->downloading_futures.erase(entry);
                        }
                        else
                        {
                            entry->second.first--;
                        }
                    },
                    this);
                // send to everyone who was waiting for the file
                for (auto &connection : fds)
                {
                    send_exactly(connection, response.to_c_str(), response.size());
                    unsent_files.insert({connection, response.begin_file_transfer()});
                }
                downloading_futures.insert({map_iterator->first, std::make_pair(fds.size(), response)});
                path_to_pid.erase(map_iterator->first);
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
                {
                    map_iterator++;
                }
                else
                {
                    fd_to_file_futures.erase(map_iterator->first);
                    map_iterator = unsent_files.erase(map_iterator);
                }
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
                f.file->close();
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
                        if (!startcmp(buffer, "exit"))
                            break;
                        fprintf(stderr, "Command not recognized!\n");
                    }
                    else if (i == listenfd)
                    {
                        newsockfd = accept(listenfd, (sockaddr *)&cli_addr, &socklen);
                        DIE(newsockfd < 0, "accept");
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
                        std::cout << response;
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
    for (auto &[path, pid] : path_to_pid)
        kill(pid, SIGKILL);
    close(udpfd);
}
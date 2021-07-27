#include "WebServer.h"
#include <algorithm>
#include <filesystem>
#include <set>

using std::set;
using std::unordered_map;

namespace fs = std::filesystem;

WebServer::WebServer(int port, const char *user, const char *pass)
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

    int option = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0;
    DIE(ret < 0, "setsockopt");

    ret = bind(listenfd, (sockaddr *)&serv_addr, sizeof(sockaddr));
    DIE(ret < 0, "bind");

    ret = listen(listenfd, MAX_CLIENTS);
    DIE(ret < 0, "listen");
    FD_SET(listenfd, &read_fds);
    fdmax = listenfd;

    FD_SET(STDIN_FILENO, &read_fds); // stdin for exit command
}

int WebServer::send_exactly(int fd, const char *buffer, int count)
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
    fd_set wait_set, tmp_wait;
    FD_ZERO(&wait_set);
    FD_SET(fd, &wait_set);

    int nr = 0;
    char *terminator = NULL;

    while (nr < max)
    {
        timeval wait_time{.tv_sec = timeout_secs, .tv_usec = timeout_micro};
        tmp_wait = wait_set;
        ret = select(fd + 1, &tmp_wait, NULL, NULL, &wait_time);
        DIE(ret < 0, "select");
        if (!FD_ISSET(fd, &tmp_wait))
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

void WebServer::close_connection(int fd)
{
    ret = close(fd);
    DIE(ret, "close");
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

int WebServer::process_http_header(int fd, char *buffer, int read_size, int header_size, char *&data, size_t &total)
{
    fd_set wait_set, tmp_wait;
    FD_ZERO(&wait_set);
    FD_SET(fd, &wait_set);

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
        size_t length = strtoul(content_length, NULL, 0);
        content_length[digits] = '\r';
        size_t content_read = read_size - header_size;
        size_t remaining = length - content_read;
        total = remaining + read_size;
        if (total > max_alloc)
            data = new char[max_alloc + 1];
        else
            data = new char[total + 1];
        memcpy(data, buffer, read_size);

        while (remaining && max_alloc - nr > 0)
        {
            timeval wait_time{.tv_sec = timeout_secs, .tv_usec = timeout_micro};
            tmp_wait = wait_set;
            ret = select(fd + 1, &tmp_wait, NULL, NULL, &wait_time);
            DIE(ret < 0, "select");
            if (!FD_ISSET(fd, &tmp_wait))
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

unordered_map<string, string> get_content_fields(char *content)
{
    unordered_map<string, string> fields;
    for (char *p = strtok(content, "&"); p; p = strtok(NULL, "&"))
    {
        char *eq = strchr(p, '=');
        if (!eq)
            return unordered_map<string, string>();
        eq[0] = 0;
        string field = p, val = eq + 1;
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

string human_readable(unsigned int size)
{
    string suffix;
    if (size >= 1 << 20)
    {
        suffix = "MB";
        size /= (1 << 20);
    }
    else if (size >= 1 << 10)
    {
        suffix = "KB";
        size /= (1 << 10);
    }
    else
    {
        suffix = "B";
    }
    return to_string(size) + suffix;
}

inline string add_table_image(string image)
{
    return "<td><img src=\"/~images/" + image + "\" height=\"20\" width=\"20\"></td>";
}

string generate_folder_html(string path)
{
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
        string size, filename = file.path().filename().string();
        if (!file.is_directory())
        {
            size = human_readable(file.file_size());
            folder += add_table_image("file.png");
            folder += "<td><a href=\"" + filename + "\">" + filename + "</a>";
        }
        else
        {
            size = "-";
            folder += add_table_image("folder.png");
            folder += "<td><a href=\"" + filename + "/\">" + filename + "</a>";
        }
        folder += "<td>" + size + "</td>";
        folder += "<td><input type=\"checkbox\" name=\"" + filename + "\"></td> ";
        folder += "</tr>";
        i++;
    }
    std::ifstream footer("html/table_footer.html", std::ios::binary);
    folder += std::string((std::istreambuf_iterator<char>(footer)), std::istreambuf_iterator<char>());
    footer.close();
    return folder;
}

string parse_url(string url)
{
    size_t pos = 0;
    while ((pos = url.find('%', pos)) != string::npos)
    {
        if (url.size() - pos <= 2)
        {
            url.erase(pos, 1);
            continue;
        }
        std::stringstream character;
        character.str(url.substr(pos + 1, 2));
        url.erase(pos, 3);
        int c;
        character >> std::hex >> c;
        url.insert(url.begin() + pos, (char)c);
    }
    return url;
}

HTTPresponse WebServer::process_http_request(char *data, int header_size, size_t read_size, size_t total_size)
{
    int content_size = total_size - header_size;
    char *content = data + header_size;
    HTTPresponse not_implemented = HTTPresponse(501).file_attachment("html/not_implemented.html", HTTPresponse::MIME::html);
    HTTPresponse not_found = HTTPresponse(404).file_attachment("html/not_found.html", HTTPresponse::MIME::html);
    const string redirect = "Redirecting...";

    Method method = get_method(data);
    char *url = strchr(data, '/');
    char *saved;
    strtok_r(url, " \r\n", &saved);
    string url_string = parse_url(string(url));

    if (strstr(url, ".."))
        return HTTPresponse(401).file_attachment(string("Incercati sa ma hackati dar in balta va inecati"), HTTPresponse::MIME::text);
    if (!strcmp(url, "/login"))
    {
        switch (method)
        {
        case Method::GET:
            return HTTPresponse(200).file_attachment("html/login.html", HTTPresponse::MIME::html);
        case Method::POST:
        {
            auto fields = get_content_fields(content);
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
        if (!check.exists() || check.is_directory())
            return not_found;
        return HTTPresponse(200).file_attachment(url + 1, HTTPresponse::MIME::png);
    }
    else if (!strcmp(url, "/favicon.ico"))
    {
        return HTTPresponse(200).file_attachment("ico/favicon.ico", HTTPresponse::MIME::icon);
    }
    else
    {
        //TODO: redirect something like "/test" to "/test/" for directory queries
        HTTPresponse login_page = HTTPresponse(302).location("/login").file_attachment(redirect, HTTPresponse::MIME::text);
        char *cookie = strstr(saved, "Cookie: ");
        if (!cookie)
            return login_page;

        cookie += strlen("Cookie: ");
        strtok_r(cookie, ";\r\n", &saved); //TODO: also check the other cookies
        char *eq = strchr(cookie, '=');
        if (!eq)
            return HTTPresponse(400).end_header();
        string cookie_str = eq + 1;
        if (cookies.find(cookie_str) == cookies.end())
            return login_page;

        string path = "files" + url_string;
        auto dir = fs::directory_entry(path);
        std::ifstream file(path, std::ios::binary);
        bool doesnt_exist = !dir.exists(); // file opens for both folders and files
        if (!doesnt_exist)
        {
            file.close();
            if (dir.is_directory() && url_string[url_string.length() - 1] != '/')
                return HTTPresponse(303).location(url_string + "/").file_attachment(redirect, HTTPresponse::MIME::text);
        }

        switch (method)
        {
        case Method::GET:
            if (doesnt_exist)
                return not_found;
            if (!dir.is_directory())
                return HTTPresponse(200).file_attachment(path.c_str(), HTTPresponse::MIME::octet_stream);
            return HTTPresponse(200).file_attachment(generate_folder_html(path), HTTPresponse::MIME::html);
        case Method::POST:
        {
            if (doesnt_exist)
            {
                //TODO: check if path to /~delete exists
                size_t pos = url_string.find("~delete");
                if (pos != std::string::npos) //check for delete
                {
                    auto fields = get_content_fields(content);
                    string remove_path = "files" + url_string.substr(0, pos);
                    std::ifstream check_exists(remove_path, std::ios::binary);
                    if (!check_exists)
                        return not_found;
                    check_exists.close();
                    for (auto &field : fields)
                        fs::remove_all(remove_path + field.first);
                    return HTTPresponse(303).location(url_string.substr(0, pos)).file_attachment(redirect, HTTPresponse::MIME::text);
                }
                return not_found;
            }
            if (!dir.is_directory())
                return not_implemented;
            data[header_size - 1] = 0;
            char *content_type = strstr(data, "Content-Type: ");
            if (content_type)
            {
                content_type = content_type + strlen("Content-Type: ");
                strtok_r(content_type, "; \r\n", &saved);
                if (!strcmp(content_type, "multipart/form-data")) // file upload
                {
                    char *boundary = strtok_r(NULL, "\r\n", &saved);
                    if (!strncmp(boundary, "boundary=", 9))
                    {
                        boundary += strlen("boundary=");
                        string boundary_str = boundary;
                        std::cout << boundary << '\n';
                    }
                    return HTTPresponse(400).end_header();
                }
            }
            auto fields = get_content_fields(content);
            //TODO: also check here (both existence of folder_name and also succesful directory creation)
            string folder_name = fields["folder_name"];
            // This check is redundant if user operates from browser, because it is also performed
            // in the javascript of the site but requests might not come from browsers
            if (folder_name.length() > 255 || folder_name.find("..") != std::string::npos)
                return HTTPresponse(303).location(url_string).file_attachment(redirect, HTTPresponse::MIME::text);
            for (auto &c : folder_name)
            {
                if (strchr("&/%~=", c)) // list of characters not accepted
                    return HTTPresponse(303).location(url_string).file_attachment(redirect, HTTPresponse::MIME::text);
            }
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
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        process_cookies();

        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                if (i == STDIN_FILENO)
                {
                    fgets(buffer, BUFLEN, stdin);
                    if (!strncmp(buffer, "exit", 4))
                        break;
                    else
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
                        close_connection(i);
                        continue;
                    }

                    char *data;
                    size_t total;
                    n = process_http_header(i, buffer, n, header_size, data, total);
                    DIE(n < 0, "process");
                    if (n == 0)
                    {
                        close_connection(i);
                        continue;
                    }

                    //std::cout << data << '\n';
                    std::cout << "\nPregatim raspunsul...\n";
                    HTTPresponse response = process_http_request(data, header_size, n, total);
                    std::cout << "Trimitem raspunsul\n";
                    std::cout << response;
                    send_exactly(i, response.to_c_str(), response.size());
                    for (response.begin_file_transfer(); response.has_more_segments(); response.next_file_segment()) //TODO: overload iterator instead of this
                    {
                        //std::cout << "Fragment nou\n";
                        send_exactly(i, response.get_file_segment(), response.segment_size());
                        //std::cout << response.get_file_segment();
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
}
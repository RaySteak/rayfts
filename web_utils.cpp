#include "web_utils.h"
#include "common_utils.h"
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

void ping(int sockfd, in_addr ip_addr)
{
    /*struct packet pkt;
    init_packet(&pkt);

    struct ether_header *eth_hdr = (struct ether_header *)pkt.payload;
    struct iphdr *ip_hdr = (struct iphdr *)(pkt.payload + IP_OFF);
    struct icmphdr *icmp_hdr = (struct icmphdr *)(pkt.payload + ICMP_OFF);

    init_eth_hdr(sockfd, eth_hdr);

    char *ip_s = inet_ntoa(ip_addr);
    printf("PING %s ...\n", ip_s);
    pkt.size = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr) + msg_len;
    ip_hdr->version = 4;
    ip_hdr->daddr = ip_addr.s_addr; //cred ca asa e cum pui adresa destinatie, nu stiu daca trebuie convertita sau nu
    ip_hdr->ttl = 255;
    get_interface_ip(sockfd, IFNAME, &(ip_hdr->saddr)); // punem adresa sursa
    //ip_hdr->saddr = ip_hdr->saddr;
    ip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr) + msg_len); // punem lungimea headerului icmp
    ip_hdr->ihl = 5;
    ip_hdr->id = getpid() & 0xFFFF;
    ip_hdr->tos = 0b00100000;
    ip_hdr->protocol = 1; //setam protocolul la icmp
    ip_hdr->frag_off = 0;
    ip_hdr->check = 0;
    ip_hdr->check = checksum(ip_hdr, sizeof(struct iphdr));
    icmp_hdr->code = 0;
    icmp_hdr->type = ICMP_ECHO;
    icmp_hdr->un.echo.id = getpid() & 0xFFFF;
    icmp_hdr->un.echo.sequence = i;
    icmp_hdr->checksum = 0;
    icmp_hdr->checksum = checksum(icmp_hdr, sizeof(struct icmphdr) + msg_len);

    struct packet reply;
    double time_elapsed = time_packet(sockfd, &pkt, &reply);

    // normalize to miliseconds
    time_elapsed = time_elapsed * 1000;

    struct iphdr *rip_hdr = (struct iphdr *)(reply.payload + IP_OFF);
    struct icmphdr *ricmp_hdr = (struct icmphdr *)(reply.payload + ICMP_OFF);

    uint16_t b_recv = ntohs(rip_hdr->tot_len) - sizeof(struct icmphdr) - sizeof(struct iphdr);
    uint16_t seq = ricmp_hdr->un.echo.sequence;
    uint8_t ttl = rip_hdr->ttl;
    printf("%d bytes from %s: icmp_seq=%hd ttl=%hhd time = %.1lf ms\n", b_recv, ip_s, seq, ttl, time_elapsed);*/
}
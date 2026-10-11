// dnsq.cpp
// A mini dig: builds a DNS query by hand, sends it over UDP to port 53
// and decodes the answer (A, AAAA and CNAME records).
// Usage: ./dnsq www.usb.ve                 (A record, server from /etc/resolv.conf)
//        ./dnsq www.usb.ve AAAA            (IPv6 addresses)
//        ./dnsq www.usb.ve A 8.8.8.8       (ask a specific server, like dig @8.8.8.8)
// Compare with: dig www.usb.ve   and watch it in Wireshark with the filter "dns"

#include "net.hpp"
#include <netdb.h>        // getaddrinfo()
#include <sys/time.h>     // timeval, for the receive timeout
#include <cstdint>        // uint8_t, uint16_t, uint32_t
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;

// ---------- reading numbers in network order (big endian) ----------
uint16_t get16(const Bytes& m, size_t p) { return (m[p] << 8) | m[p + 1]; }
uint32_t get32(const Bytes& m, size_t p) { return (uint32_t(get16(m, p)) << 16) | get16(m, p + 2); }

// ---------- 1. build the query ----------
// Header (12 bytes) + question: name as labels, type, class.
Bytes build_query(uint16_t id, const std::string& name, uint16_t type) {
    Bytes q = {
        uint8_t(id >> 8), uint8_t(id),   // ID: to match the answer with the question
        0x01, 0x00,                      // flags: RD = 1 (recursion desired)
        0, 1,                            // QDCOUNT: 1 question
        0, 0,  0, 0,  0, 0               // ANCOUNT, NSCOUNT, ARCOUNT: 0
    };
    std::stringstream ss(name);
    std::string label;
    while (std::getline(ss, label, '.')) {          // "www.usb.ve" -> www, usb, ve
        if (label.empty()) continue;
        q.push_back(label.size());                  // length byte
        q.insert(q.end(), label.begin(), label.end());
    }
    q.push_back(0);                                 // end of the name
    q.push_back(type >> 8);  q.push_back(type & 0xFF);   // QTYPE (1 = A, 28 = AAAA)
    q.push_back(0);          q.push_back(1);             // QCLASS 1 = IN (Internet)
    return q;
}

// ---------- 2. read names (they may be compressed) ----------
// A name is a list of labels ending in 0, OR a 2-byte pointer (first two bits 11)
// to a name written earlier in the message.
std::string read_name(const Bytes& m, size_t p) {
    std::string name;
    for (int jumps = 0; p < m.size() && jumps < 20; ) {
        uint8_t len = m[p];
        if (len == 0) break;
        if ((len & 0xC0) == 0xC0) {                 // pointer: jump to that offset
            if (p + 1 >= m.size()) break;
            p = ((len & 0x3F) << 8) | m[p + 1];
            ++jumps;
            continue;
        }
        if (p + 1 + len > m.size()) break;
        if (!name.empty()) name += '.';
        name.append(reinterpret_cast<const char*>(&m[p + 1]), len);
        p += 1 + len;
    }
    return name;
}

// Position right after a name (without following pointers).
size_t skip_name(const Bytes& m, size_t p) {
    while (p < m.size()) {
        uint8_t len = m[p];
        if (len == 0) return p + 1;
        if ((len & 0xC0) == 0xC0) return p + 2;
        p += 1 + len;
    }
    return m.size();
}

// ---------- 3. print the answers ----------
void print_answers(const Bytes& m) {
    int rcode = m[3] & 0x0F;
    if (rcode != 0) {
        std::cout << "error: rcode " << rcode << (rcode == 3 ? " (NXDOMAIN: the name does not exist)" : "") << "\n";
        return;
    }
    if (m[2] & 0x02) std::cout << "warning: truncated answer (TC bit); dig would retry over TCP\n";

    uint16_t qd = get16(m, 4), an = get16(m, 6);
    size_t p = 12;                                   // after the header
    for (int i = 0; i < qd; ++i) p = skip_name(m, p) + 4;   // skip the question(s)

    for (int i = 0; i < an && p + 10 <= m.size(); ++i) {
        std::string owner = read_name(m, p);
        p = skip_name(m, p);
        uint16_t type  = get16(m, p);
        uint32_t ttl   = get32(m, p + 4);
        uint16_t rdlen = get16(m, p + 8);
        p += 10;
        if (p + rdlen > m.size()) break;

        char ip[INET6_ADDRSTRLEN];
        std::cout << owner << "\t" << ttl << "\t";
        if (type == 1 && rdlen == 4)        std::cout << "A\t"     << inet_ntop(AF_INET,  &m[p], ip, sizeof ip);
        else if (type == 28 && rdlen == 16) std::cout << "AAAA\t"  << inet_ntop(AF_INET6, &m[p], ip, sizeof ip);
        else if (type == 5)                 std::cout << "CNAME\t" << read_name(m, p);
        else                                std::cout << "type " << type << "\t(" << rdlen << " bytes)";
        std::cout << "\n";
        p += rdlen;                                  // next record
    }
}

// First "nameserver" of /etc/resolv.conf (the same server getaddrinfo uses).
std::string default_server() {
    std::ifstream f("/etc/resolv.conf");
    std::string word, ip;
    while (f >> word)
        if (word == "nameserver" && f >> ip) return ip;
    return "127.0.0.1";                             // the C library's default
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: " << argv[0] << " <name> [A|AAAA] [server]\n";
        return 1;
    }
    std::string name   = argv[1];
    uint16_t    type   = (argc > 2 && std::string(argv[2]) == "AAAA") ? 28 : 1;
    std::string server = (argc > 3) ? argv[3] : default_server();

    // Where to send: server:53 over UDP (the server may be an IPv4 or IPv6 address).
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;                 // UDP
    hints.ai_flags    = AI_NUMERICHOST;             // the server must be an IP, no DNS here
    addrinfo* res = nullptr;
    int err = getaddrinfo(server.c_str(), "53", &hints, &res);
    if (err != 0) { std::cerr << "bad server " << server << ": " << gai_strerror(err) << "\n"; return 1; }

    Socket sock(socket(res->ai_family, SOCK_DGRAM, 0));
    if (!sock.ok()) { perror("socket"); freeaddrinfo(res); return 1; }

    // UDP may lose the datagram and never tell us: wait at most 2 seconds per try.
    timeval tv{2, 0};
    setsockopt(sock.fd(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    uint16_t id = std::random_device{}() & 0xFFFF;
    Bytes query = build_query(id, name, type);
    Bytes reply(4096);
    ssize_t n = -1;

    // UDP does not retransmit: if no answer arrives, WE send again (3 tries).
    for (int attempt = 1; attempt <= 3 && n < 0; ++attempt) {
        sendto(sock.fd(), query.data(), query.size(), 0, res->ai_addr, res->ai_addrlen);
        n = recvfrom(sock.fd(), reply.data(), reply.size(), 0, nullptr, nullptr);
        if (n < 0) std::cerr << "try " << attempt << ": no answer (" << server << ")\n";
    }
    freeaddrinfo(res);
    if (n < 12) { std::cerr << "no valid answer\n"; return 1; }
    reply.resize(n);

    if (get16(reply, 0) != id) { std::cerr << "answer with a different ID: ignored\n"; return 1; }

    std::cout << ";; server " << server << ", query " << query.size()
              << " bytes, answer " << n << " bytes\n";
    print_answers(reply);
}

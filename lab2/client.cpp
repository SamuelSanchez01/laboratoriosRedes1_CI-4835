// client.cpp
// A TCP client that accepts host NAMES, not just IPs: it uses getaddrinfo (DNS)
// and tries every address it gets (IPv6 and IPv4) until one connects.
// Usage: ./client localhost 5000
//        ./client 127.0.0.1 5000
//        ./client ::1 5000

#include "net.hpp"
#include <netdb.h>        // getaddrinfo(), freeaddrinfo(), gai_strerror()
#include <iostream>
#include <string>

// Resolves host:port and connects to the first address that answers.
Socket connect_to(const char* host, const char* port) {
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;     // IPv4 or IPv6, whatever the name has
    hints.ai_socktype = SOCK_STREAM;   // TCP

    addrinfo* res = nullptr;
    int err = getaddrinfo(host, port, &hints, &res);   // now we also pass the port
    if (err != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(err) << "\n";
        return Socket(-1);
    }

    Socket sock(-1);
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        std::cout << "trying " << addr_to_string(p->ai_addr) << " ... " << std::flush;
        Socket attempt(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (attempt.ok() && connect(attempt.fd(), p->ai_addr, p->ai_addrlen) == 0) {
            std::cout << "connected\n";
            sock = std::move(attempt);   // keep this one (move assignment)
            break;
        }
        perror("failed");                // e.g. "failed: Connection refused"
    }                                     // a failed attempt is closed here by ~Socket()

    freeaddrinfo(res);
    return sock;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    Socket sock = connect_to(argv[1], argv[2]);
    if (!sock.ok()) return 1;

    LineReader in(sock.fd());
    std::string line, reply;
    while (std::getline(std::cin, line)) {             // Ctrl+D ends
        if (!send_all(sock.fd(), line + "\n")) { perror("send"); break; }
        if (!in.read_line(reply)) {                     // waits for a WHOLE line
            std::cout << "(server closed the connection)\n";
            break;
        }
        std::cout << reply << "\n";
    }
}   // ~Socket() closes the connection (FIN)

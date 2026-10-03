// resolver.cpp
// A mini nslookup: prints every IPv4 and IPv6 address of a host name.
// Usage: ./resolver www.usb.ve
// Compare the output with: dig www.usb.ve   and   dig AAAA www.usb.ve

#include <netdb.h>       // getaddrinfo(), freeaddrinfo(), gai_strerror()
#include <arpa/inet.h>   // inet_ntop(), INET6_ADDRSTRLEN
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <hostname>\n";
        return 1;
    }

    // 1. Hints: tell getaddrinfo what kind of answers we want.
    addrinfo hints{};                  // {} zero-initializes every field
    hints.ai_family   = AF_UNSPEC;     // both IPv4 (AF_INET) and IPv6 (AF_INET6)
    hints.ai_socktype = SOCK_STREAM;   // one entry per address (otherwise TCP, UDP and RAW repeat it)

    // 2. Resolve the name. On success, res points to a linked list of results.
    addrinfo* res = nullptr;
    int err = getaddrinfo(argv[1], nullptr, &hints, &res);   // nullptr: no service/port needed
    if (err != 0) {
        // getaddrinfo has its own error codes: use gai_strerror, not perror.
        std::cerr << "getaddrinfo: " << gai_strerror(err) << "\n";
        return 1;
    }

    // 3. Walk the linked list through ai_next.
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        char ip[INET6_ADDRSTRLEN];   // 46 bytes: big enough for IPv4 and IPv6 text

        // ai_addr is a generic sockaddr*; the real type depends on the family.
        void* addr;
        if (p->ai_family == AF_INET) {
            addr = &reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr;     // 32-bit address
        } else {
            addr = &reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr;   // 128-bit address
        }

        // 4. Binary address -> printable text ("n" = network, "p" = presentation).
        inet_ntop(p->ai_family, addr, ip, sizeof ip);

        std::cout << (p->ai_family == AF_INET ? "IPv4 " : "IPv6 ") << ip << "\n";
    }

    // 5. getaddrinfo allocated the list, so we must free it.
    freeaddrinfo(res);
    return 0;
}

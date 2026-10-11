// udp_echo.cpp
// A UDP echo server: every datagram that arrives is sent back to whoever sent it.
// No connect, no listen, no accept: UDP has no connections.
// Usage: ./udp_echo 6000
// Test:  nc -u 127.0.0.1 6000      (type lines; each line is one datagram)

#include "net.hpp"
#include <cstdio>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 6000;

    // 1. SOCK_DGRAM = datagrams = UDP.
    Socket sock(socket(AF_INET, SOCK_DGRAM, 0));
    if (!sock.ok()) { perror("socket"); return 1; }

    // 2. Bind to a port so clients know where to send.
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        perror("bind");
        return 1;
    }
    std::cout << "UDP echo on port " << port << std::endl;

    // 3. One socket serves everybody: recvfrom tells us WHO sent each datagram.
    for (;;) {
        char buf[1500];                     // a typical Ethernet MTU
        sockaddr_storage from{};
        socklen_t len = sizeof from;
        ssize_t n = recvfrom(sock.fd(), buf, sizeof buf, 0,
                             reinterpret_cast<sockaddr*>(&from), &len);
        if (n < 0) { perror("recvfrom"); continue; }

        std::cout << addr_to_string(reinterpret_cast<sockaddr*>(&from))
                  << " sent " << n << " bytes" << std::endl;

        // 4. Answer to the same address the datagram came from.
        sendto(sock.fd(), buf, n, 0, reinterpret_cast<sockaddr*>(&from), len);
    }
}

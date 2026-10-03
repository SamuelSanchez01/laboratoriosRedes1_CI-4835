// client.cpp
// A TCP client: connects to IP:port, sends each line typed on stdin
// and prints whatever the server answers.
// Usage: ./client 127.0.0.1 5000
// Test:  in another terminal run  nc -l 5000  and answer from there.

#include "socket.hpp"
#include <arpa/inet.h>    // sockaddr_in, htons(), inet_pton()
#include <sys/socket.h>   // socket(), connect(), send(), recv()
#include <cstdio>         // perror()
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }

    // 1. Create a TCP socket over IPv4.
    //    The Socket object owns the fd and will close it automatically.
    Socket sock(socket(AF_INET, SOCK_STREAM, 0));
    if (!sock.ok()) {
        perror("socket");
        return 1;
    }

    // 2. Fill in the server address.
    sockaddr_in server{};                                  // zero everything first
    server.sin_family = AF_INET;
    server.sin_port   = htons(std::stoi(argv[2]));         // host byte order -> network (big endian)
    if (inet_pton(AF_INET, argv[1], &server.sin_addr) != 1) {   // "127.0.0.1" -> 4 bytes
        std::cerr << "invalid IPv4 address: " << argv[1] << "\n";
        return 1;
    }

    // 3. Connect. The three-way handshake (SYN, SYN-ACK, ACK) happens here.
    if (connect(sock.fd(), reinterpret_cast<sockaddr*>(&server), sizeof server) < 0) {
        perror("connect");   // e.g. "Connection refused" if nobody is listening
        return 1;
    }

    // 4. Loop: read a line, send it, wait for the reply, print it.
    std::string line;
    char buf[1024];
    while (std::getline(std::cin, line)) {   // ends with Ctrl+D
        line += '\n';                        // getline drops the newline; put it back

        if (send(sock.fd(), line.data(), line.size(), 0) < 0) {
            perror("send");
            break;
        }

        // recv returns how many bytes arrived. TCP is a byte stream:
        // one recv is NOT guaranteed to be one whole message.
        ssize_t n = recv(sock.fd(), buf, sizeof buf, 0);
        if (n < 0) {
            perror("recv");
            break;
        }
        if (n == 0) {   // 0 means the other side closed the connection (FIN)
            std::cout << "(server closed the connection)\n";
            break;
        }
        std::cout.write(buf, n);   // buf is not null-terminated: write exactly n bytes
    }

    return 0;
}   // sock goes out of scope here: ~Socket() calls close() and the FIN is sent

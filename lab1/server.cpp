// server.cpp
// A TCP echo server: sends back every byte it receives.
// Serves one client at a time (handling many clients comes in Lab II).
// Usage: ./server 5000      (port is optional, default 5000)
// Test:  ./client 127.0.0.1 5000   or   nc 127.0.0.1 5000
// Check: ss -tlnp | grep 5000

#include "socket.hpp"
#include <arpa/inet.h>    // sockaddr_in, htons(), htonl(), INADDR_ANY
#include <sys/socket.h>   // socket(), setsockopt(), bind(), listen(), accept()
#include <cstdio>         // perror()
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 5000;

    // 1. Create the listening socket.
    Socket listener(socket(AF_INET, SOCK_STREAM, 0));
    if (!listener.ok()) {
        perror("socket");
        return 1;
    }

    // 2. Allow reusing the port right after a restart.
    //    Without this, bind fails with "Address already in use"
    //    while old connections are still in TIME_WAIT.
    int on = 1;
    if (setsockopt(listener.fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0) {
        perror("setsockopt");
        return 1;
    }

    // 3. Bind to every local interface (0.0.0.0) on the given port.
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);         // 16-bit: htons
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 32-bit: htonl
    if (bind(listener.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        perror("bind");
        return 1;
    }

    // 4. Start listening. 8 = how many pending connections the kernel may queue.
    if (listen(listener.fd(), 8) < 0) {
        perror("listen");
        return 1;
    }
    std::cout << "listening on port " << port << "\n";

    // 5. Accept clients forever, one at a time.
    for (;;) {
        // accept blocks until a client connects and returns a NEW descriptor
        // for that conversation; the listener keeps listening.
        Socket client(accept(listener.fd(), nullptr, nullptr));
        if (!client.ok()) {
            perror("accept");
            continue;
        }

        // 6. Echo loop: whatever arrives is sent straight back.
        char buf[1024];
        ssize_t n;
        while ((n = recv(client.fd(), buf, sizeof buf, 0)) > 0) {
            if (send(client.fd(), buf, n, 0) < 0) {
                perror("send");
                break;
            }
        }
        if (n < 0) perror("recv");
        // n == 0: the client closed the connection.
    }   // client goes out of scope at the end of each iteration: its fd is closed
}

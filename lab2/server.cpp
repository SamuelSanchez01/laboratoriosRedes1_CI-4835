// server.cpp
// A concurrent TCP echo server: one thread per client.
// It listens on IPv6 AND IPv4 at the same time (dual stack) when the machine has IPv6:
// IPv4 clients then show up as IPv4-mapped addresses, e.g. [::ffff:127.0.0.1].
// Usage: ./server 5000      (port is optional, default 5000)
// Test:  ./client localhost 5000   (open several at once)

#include "net.hpp"
#include <netinet/in.h>   // sockaddr_in6, in6addr_any, IPPROTO_IPV6, IPV6_V6ONLY
#include <cstdio>         // perror()
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

std::mutex out_mutex;   // std::cout is shared by all threads: one writer at a time

void log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(out_mutex);   // unlocked automatically at the end (RAII)
    std::cout << msg << std::endl;
}

// Runs in its own thread: talks to ONE client until it disconnects.
void handle_client(Socket client, std::string who) {
    LineReader in(client.fd());
    std::string line;
    while (in.read_line(line)) {                   // one whole line at a time
        log(who + " says: " + line);
        if (!send_all(client.fd(), line + "\n")) break;   // echo it back
    }
    log(who + " disconnected");
}   // client goes out of scope here: its fd is closed

// Creates the listening socket. First tries IPv6 "dual stack" (IPv6 + IPv4 on one socket);
// if this machine has no IPv6, falls back to IPv4 only.
Socket make_listener(int port) {
    int on = 1, off = 0;
    Socket s(socket(AF_INET6, SOCK_STREAM, 0));
    if (s.ok()) {
        setsockopt(s.fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        setsockopt(s.fd(), IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off);   // accept IPv4 too
        sockaddr_in6 a{};
        a.sin6_family = AF_INET6;
        a.sin6_port   = htons(port);
        a.sin6_addr   = in6addr_any;                 // "::" = every interface
        if (bind(s.fd(), reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) return Socket(-1);
    } else {                                         // no IPv6 here: plain IPv4
        s = Socket(socket(AF_INET, SOCK_STREAM, 0));
        if (!s.ok()) return s;
        setsockopt(s.fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        sockaddr_in a{};
        a.sin_family      = AF_INET;
        a.sin_port        = htons(port);
        a.sin_addr.s_addr = htonl(INADDR_ANY);       // "0.0.0.0" = every interface
        if (bind(s.fd(), reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) return Socket(-1);
    }
    if (listen(s.fd(), 16) < 0) return Socket(-1);
    return s;
}

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 5000;

    // 1-4. socket + options + bind + listen.
    Socket listener = make_listener(port);
    if (!listener.ok()) { perror("listen socket"); return 1; }
    log("listening on port " + std::to_string(port));

    // 5. Accept forever. Each client gets its own thread, so accept() is called again right away.
    for (;;) {
        sockaddr_storage peer{};            // big enough for IPv4 or IPv6
        socklen_t len = sizeof peer;
        Socket client(accept(listener.fd(), reinterpret_cast<sockaddr*>(&peer), &len));
        if (!client.ok()) { perror("accept"); continue; }

        std::string who = addr_to_string(reinterpret_cast<sockaddr*>(&peer));
        log(who + " connected");

        // 6. The thread takes ownership of the socket (std::move) and runs on its own (detach).
        std::thread(handle_client, std::move(client), who).detach();
    }
}

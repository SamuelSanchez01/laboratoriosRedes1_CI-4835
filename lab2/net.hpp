// net.hpp
// Small helpers shared by every program of Lab II.
#pragma once

#include "socket.hpp"
#include <arpa/inet.h>    // inet_ntop(), ntohs(), sockaddr_in, sockaddr_in6
#include <sys/socket.h>   // send(), recv()
#include <cerrno>         // errno, EINTR
#include <string>

// Sends EVERY byte of data.
// send() may accept fewer bytes than we asked for, so we loop until all are out.
// MSG_NOSIGNAL: if the other side already closed, return an error
// instead of killing the whole process with SIGPIPE.
inline bool send_all(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;   // interrupted by a signal: just retry
            return false;
        }
        sent += n;
    }
    return true;
}

// Reads one line at a time from a TCP socket.
// TCP is a byte stream: one recv() can bring half a line, or three lines at once.
// We keep the extra bytes in buf_ until a '\n' shows up.
class LineReader {
    int fd_;
    std::string buf_;   // bytes received but not yet returned

public:
    explicit LineReader(int fd) : fd_(fd) {}

    // Puts the next line (without '\n') in line. Returns false when the connection ends.
    bool read_line(std::string& line) {
        for (;;) {
            size_t pos = buf_.find('\n');
            if (pos != std::string::npos) {            // a whole line is already here
                line = buf_.substr(0, pos);
                buf_.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();   // "\r\n" endings
                return true;
            }
            char tmp[1024];
            ssize_t n = recv(fd_, tmp, sizeof tmp, 0);
            if (n <= 0) return false;                  // 0 = closed (FIN), < 0 = error
            buf_.append(tmp, n);                       // keep the bytes and look again
        }
    }
};

// Turns an IPv4 or IPv6 socket address into "ip:port" text.
inline std::string addr_to_string(const sockaddr* sa) {
    char ip[INET6_ADDRSTRLEN];
    if (sa->sa_family == AF_INET) {
        auto* a = reinterpret_cast<const sockaddr_in*>(sa);
        inet_ntop(AF_INET, &a->sin_addr, ip, sizeof ip);
        return std::string(ip) + ":" + std::to_string(ntohs(a->sin_port));
    }
    auto* a = reinterpret_cast<const sockaddr_in6*>(sa);
    inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof ip);
    return "[" + std::string(ip) + "]:" + std::to_string(ntohs(a->sin6_port));
}

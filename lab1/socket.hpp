// socket.hpp
// A tiny RAII wrapper around a socket file descriptor.
// The descriptor is closed automatically when the object goes out of scope,
// so we never forget to call close() (in C you must call it by hand on every exit path).
#pragma once

#include <unistd.h>   // close()

class Socket {
    int fd_ = -1;   // -1 means "no descriptor owned"

public:
    // Takes ownership of a descriptor returned by socket() or accept().
    explicit Socket(int fd) : fd_(fd) {}

    // RAII: the destructor releases the resource.
    ~Socket() {
        if (fd_ >= 0) ::close(fd_);
    }

    // No copies: two objects owning the same fd would close it twice.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Moves are allowed: ownership passes to the new object,
    // and the old one is left empty (-1) so its destructor does nothing.
    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) ::close(fd_);   // release what we had before
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    // Raw descriptor, to pass to connect(), send(), recv(), ...
    int fd() const { return fd_; }

    // True if socket()/accept() succeeded.
    bool ok() const { return fd_ >= 0; }
};

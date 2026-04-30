#ifndef NETPULSE_SOCKET_WRAPPER_H
#define NETPULSE_SOCKET_WRAPPER_H

#include <unistd.h>

#include <utility>

class SocketWrapper {
private:
    int fd_{-1};

public:
    // Constructors & Destructor
    SocketWrapper();

    explicit SocketWrapper(int fd);

    ~SocketWrapper();

    // Copy Semantics (Deleted to prevent double-close)
    SocketWrapper(const SocketWrapper &) = delete;

    SocketWrapper &operator=(const SocketWrapper &) = delete;

    // Move Semantics (Transfers ownership)
    SocketWrapper(SocketWrapper &&other) noexcept;

    SocketWrapper &operator=(SocketWrapper &&other) noexcept;

    // Accessors
    [[nodiscard]] int getFd() const;

    bool operator==(int other) const noexcept;

    explicit operator bool() const noexcept;
};

#endif  // NETPULSE_SOCKET_WRAPPER_H

#ifndef NETPULSE_CLIENT_SESSION_H
#define NETPULSE_CLIENT_SESSION_H

#include <iostream>
#include <string>
#include <thread>

#include "line_framer.h"
#include "message.h"
#include "protocol.h"
#include "socket_wrapper.h"
#include "thread_safe_queue.h"
class ClientSession {
private:
    static constexpr std::size_t kMaxBufferBytes = 16 * 1024;
    SocketWrapper client_sock_;

    LineFramer framer_;
    ThreadSafeQueue<Message> &server_inbox_;
    ThreadSafeQueue<Message> outbox_;

    std::jthread writer_thread_;

    std::atomic<bool> alive_{true};

    void writeLoop();

public:
    ClientSession(SocketWrapper client_sock,
                  ThreadSafeQueue<Message> &server_inbox);
    ~ClientSession() = default;

    ClientSession(const ClientSession &) = delete;
    ClientSession &operator=(const ClientSession &) = delete;

    const SocketWrapper *getClientSock() const;
    void readLoop();
    void deliver(const Message &msg);

    bool isAlive() const;
};

#endif  // NETPULSE_CLIENT_SESSION_H

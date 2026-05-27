#ifndef NETPULSE_SERVER_H
#define NETPULSE_SERVER_H

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include "dispatcher.h"
#include "message.h"
#include "socket_wrapper.h"
#include "thread_safe_queue.h"

/* Owns the whole server lifecycle: the listen socket, the message inbox, the
// dispatcher, and the per-client reader threads. run() blocks accepting clients
// until SIGINT, then tears everything down in order and returns.
*/
class Server {
public:
    explicit Server(std::uint16_t port);

    // Accept loop until SIGINT; then stop the dispatcher, wake + drain all
    // client connections, wait for every reader thread to finish, and return.
    void run();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

private:
    SocketWrapper listen_sock_;
    ThreadSafeQueue<Message> inbox_;
    Dispatcher dispatcher_;
    std::uint16_t port_;

    // Tracks how many reader threads are live so shutdown can wait for them all
    // to finish before the Server (and its inbox_) is destroyed.
    std::mutex readers_mutex_;
    std::condition_variable readers_cv_;
    std::size_t active_readers_ = 0;  // guarded by readers_mutex_
};

#endif  // NETPULSE_SERVER_H
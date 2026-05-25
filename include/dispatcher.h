#ifndef NETPULSE_DISPATCHER_H
#define NETPULSE_DISPATCHER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "client_session.h"
#include "message.h"
#include "nickname_registry.h"
#include "room_manager.h"
#include "thread_safe_queue.h"

// The Dispatcher is the server's single-threaded brain. Exactly one thread
// runs run(), draining the shared inbox and processing one Message at a time.
// Because all room/registry/connection mutations happen on that one thread,
// there are no cross-component races to reason about between dispatch steps —
// the only locks that matter are the small ones inside each component and the
// connection-table mutex (touched by the acceptor thread on connect).
class Dispatcher {
public:
    explicit Dispatcher(ThreadSafeQueue<Message> &inbox);

    // Called by the acceptor thread when a client connects, before its reader
    // thread starts. The connection table is the one structure shared between
    // the acceptor and the dispatcher, so it has its own mutex.
    void registerSession(std::shared_ptr<ClientSession> session);

    // Blocks forever, draining the inbox. (Graceful shutdown is Phase 4.)
    void run();

    // Processes a single message. This is the unit-test seam: feed it Messages
    // with a sender_fd of a registered (mock) session and observe deliveries.
    void handle(const Message &msg);

    Dispatcher(const Dispatcher &) = delete;
    Dispatcher &operator=(const Dispatcher &) = delete;
    Dispatcher(Dispatcher &&) = delete;
    Dispatcher &operator=(Dispatcher &&) = delete;

private:
    void handleNick(const Message &msg);
    void handleJoin(const Message &msg);
    void handleMsg(const Message &msg);
    void handleDm(const Message &msg);
    void handleList(int fd);
    void handleQuit(int fd);  // also the disconnect-cleanup path

    // Auth guard: true if fd has claimed a nick; otherwise sends ERR and false.
    bool requireNick(int fd);

    void sendOk(int fd, const std::string &context);
    void sendError(int fd, const std::string &reason);

    // Looks up a session by fd under the connection mutex, returning a strong
    // ref (copy) so the caller can use it after releasing the lock.
    std::shared_ptr<ClientSession> lookup(int fd);

    ThreadSafeQueue<Message> &inbox_;
    RoomManager rooms_;
    NicknameRegistry registry_;

    mutable std::mutex connections_mutex_;
    std::unordered_map<int, std::shared_ptr<ClientSession>> connections_;
};

#endif  // NETPULSE_DISPATCHER_H
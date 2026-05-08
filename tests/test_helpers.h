#ifndef NETPULSE_TEST_HELPERS_H
#define NETPULSE_TEST_HELPERS_H

#include <memory>
#include <vector>
#include <mutex>
#include "../include/client_session.h"
#include "../include/socket_wrapper.h"
#include "../include/thread_safe_queue.h"

inline ThreadSafeQueue<Message> dummy_server_inbox;

class MockClientSession : public ClientSession {
public:
    std::vector<Message> received_messages;
    std::mutex mock_mutex;

    explicit MockClientSession(int fd) : ClientSession(SocketWrapper(fd), dummy_server_inbox) {}

    void deliver(const Message& msg) override {
        std::lock_guard<std::mutex> lock(mock_mutex);
        received_messages.push_back(msg);
    }
};

inline std::shared_ptr<MockClientSession> make_dummy_client(int fd) {
    return std::make_shared<MockClientSession>(fd);
}

#endif // NETPULSE_TEST_HELPERS_H
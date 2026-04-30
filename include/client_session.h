#ifndef NETPULSE_CLIENT_SESSION_H
#define NETPULSE_CLIENT_SESSION_H

#include <thread>
#include <string>
#include <iostream>
#include "socket_wrapper.h"
#include "thread_safe_queue.h"

class client_session {
private:
    SocketWrapper client_sock_;

    ThreadSafeQueue<std::string>& server_inbox_;
    ThreadSafeQueue<std::string> outbox_;

    std::jthread writer_thread_;

    void writeLoop();

public:
    client_session(SocketWrapper client_sock, ThreadSafeQueue<std::string>& server_inbox);
    ~client_session()=default;

    client_session(const client_session&)=delete;
    client_session& operator=(const client_session&)=delete;

    void readLoop();
    void deliver(const std::string& msg);

};



#endif //NETPULSE_CLIENT_SESSION_H

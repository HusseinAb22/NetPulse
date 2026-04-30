#ifndef NETPULSE_CLIENT_SESSION_H
#define NETPULSE_CLIENT_SESSION_H

#include <thread>
#include <string>
#include <iostream>
#include "socket_wrapper.h"
#include "thread_safe_queue.h"

class ClientSession {
private:
    SocketWrapper client_sock_;

    ThreadSafeQueue<std::string>& server_inbox_;
    ThreadSafeQueue<std::string> outbox_;

    std::jthread writer_thread_;

    void writeLoop();

public:
    ClientSession(SocketWrapper client_sock, ThreadSafeQueue<std::string>& server_inbox);
    ~ClientSession()=default;

    ClientSession(const ClientSession&)=delete;
    ClientSession& operator=(const ClientSession&)=delete;

    void readLoop();
    void deliver(const std::string& msg);

};



#endif //NETPULSE_CLIENT_SESSION_H

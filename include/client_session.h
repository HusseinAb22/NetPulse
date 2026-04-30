#ifndef NETPULSE_CLIENT_SESSION_H
#define NETPULSE_CLIENT_SESSION_H

#include <thread>
#include <string>
#include <iostream>
#include "socket_wrapper.h"
#include "thread_safe_queue.h"
#include "message.h"

class ClientSession {
private:
    SocketWrapper client_sock_;

    ThreadSafeQueue<Message>& server_inbox_;
    ThreadSafeQueue<Message> outbox_;

    std::jthread writer_thread_;

    void writeLoop();

public:
    ClientSession(SocketWrapper client_sock, ThreadSafeQueue<Message>& server_inbox);
    ~ClientSession()=default;

    ClientSession(const ClientSession&)=delete;
    ClientSession& operator=(const ClientSession&)=delete;

    const SocketWrapper *getClientSock() const;
    void readLoop();
    void deliver(const Message &msg);

};



#endif //NETPULSE_CLIENT_SESSION_H

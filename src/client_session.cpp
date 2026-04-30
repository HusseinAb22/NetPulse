#include "../include/client_session.h"
#include <sys/socket.h>
#include <cstring>



ClientSession::ClientSession(SocketWrapper client_sock, ThreadSafeQueue<Message>& server_inbox): client_sock_(std::move(client_sock)), server_inbox_(server_inbox), writer_thread_([this]()  {writeLoop(); }){
    std::cout << "new Client Session created!" << std::endl;
}

void ClientSession::deliver(const Message &msg){
    this->outbox_.push(msg);
}

void ClientSession::writeLoop() {
    while(true) {
        auto msg = this->outbox_.pop();

        ssize_t bytes_sent = send(client_sock_.getFd(), msg.text.c_str(), msg.text.length(),0);
        if(bytes_sent < 0) {
            std::cerr << "ClientSession: Client send error or client disconnected "  << std::endl;
            break;
        }
    }
}

void ClientSession::readLoop(){
    char buffer[1024];
    while(true) {
        const ssize_t bytes = recv(client_sock_.getFd(), buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            std::cerr << "Error reading from socket" << std::endl;
            break;
        }
        const std::string message(buffer, bytes);
        Message msg{client_sock_.getFd(),message};
        this->server_inbox_.push(msg);
    }
}

const SocketWrapper *ClientSession::getClientSock() const{
    return &this->client_sock_;
}
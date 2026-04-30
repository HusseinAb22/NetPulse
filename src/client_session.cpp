#include "../include/client_session.h"
#include <sys/socket.h>
#include <cstring>



client_session::client_session(SocketWrapper client_sock, ThreadSafeQueue<std::string>& server_inbox): client_sock_(std::move(client_sock)), server_inbox_(server_inbox), writer_thread_([this]()  {writeLoop(); }){
    std::cout << "new Client Session created!" << std::endl;
}
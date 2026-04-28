//
// Created by husse on 4/28/2026.
//

#include "../include/socket_wrapper.h"

SocketWrapper::SocketWrapper() {

}
SocketWrapper::~SocketWrapper() {
    if (this->fd_!=-1) {
        close(fd_);
    }
}

SocketWrapper::SocketWrapper(const int fd) : fd_(fd) {

}

SocketWrapper::SocketWrapper(SocketWrapper &&other) noexcept :fd_(std::exchange(other.fd_, -1)){

}

SocketWrapper& SocketWrapper::operator=(SocketWrapper &&other) noexcept {
    if (this != &other) {
        SocketWrapper temp(std::move(other));
        std::swap(this->fd_, temp.fd_);
    }
    return *this;
}

int SocketWrapper::getFd() const {
    return fd_;
}

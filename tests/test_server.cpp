#include "../include/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <future>
#include <thread>

namespace {
int connectClient(std::uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    timeval tv{2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    for (int i = 0; i < 100; ++i) {
        if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
            return fd;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    close(fd);
    return -1;
}
}  // namespace

TEST(ServerTest, ShutsDownOnSigint) {
    const std::uint16_t port = 9124;
    Server server(port);
    auto fut = std::async(std::launch::async, [&] { server.run(); });

    const int client = connectClient(port);
    ASSERT_GE(client, 0);

    const char *nick = "NICK alice\n";
    send(client, nick, std::strlen(nick), 0);
    char buf[128];
    const ssize_t n = recv(client, buf, sizeof(buf), 0);
    EXPECT_GT(n, 0);  // got the OK -> server up, handler installed

    std::raise(SIGINT);

    const auto status = fut.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);  // run() returned
    fut.get();

    close(client);
}

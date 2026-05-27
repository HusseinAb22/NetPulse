#include "../include/server.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include "../include/client_session.h"

namespace {
// Set by the SIGINT handler. A handler may only safely touch a lock-free
// atomic, so it does exactly that. The accept loop polls with a short timeout
// and notices the flag — we deliberately do NOT shutdown() the listen socket
// to wake accept(), because that works on Linux but is a no-op on macOS/BSD.
std::atomic<bool> g_shutdown{false};
}  // namespace

extern "C" void netpulse_on_sigint(int /*signum*/) {
    g_shutdown.store(true);
}

namespace {
void installSigintHandler() {
    struct sigaction sa{};
    sa.sa_handler = netpulse_on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // NO SA_RESTART, so a blocked syscall is interrupted
    sigaction(SIGINT, &sa, nullptr);
}
}  // namespace

Server::Server(std::uint16_t port)
    : listen_sock_(socket(AF_INET, SOCK_STREAM, 0)),
      dispatcher_(inbox_),
      port_(port) {
    if (!listen_sock_) {
        throw std::runtime_error("socket() failed");
    }

    constexpr int yes{1};
    setsockopt(listen_sock_.getFd(), SOL_SOCKET, SO_REUSEADDR, &yes,
               sizeof(yes));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock_.getFd(), reinterpret_cast<sockaddr *>(&addr),
             sizeof(addr)) < 0) {
        throw std::runtime_error("bind() failed");
    }
    listen(listen_sock_.getFd(), 10);
}

void Server::run() {
    std::signal(SIGPIPE, SIG_IGN);
    installSigintHandler();

    // The dispatcher's single thread; request_stop() at shutdown ends run().
    std::jthread dispatch_thread(
        [this](std::stop_token st) { dispatcher_.run(st); });

    std::cout << "server listening on port " << port_ << " (Ctrl-C to stop)"
              << std::endl;

    while (!g_shutdown.load()) {
        // Poll with a short timeout so we periodically re-check g_shutdown.
        // Portable wakeup: unlike shutdown() on a listening socket, poll()
        // behaves the same on Linux and macOS.
        struct pollfd pfd = {};
        pfd.fd = listen_sock_.getFd();
        pfd.events = POLLIN;
        const int ready = ::poll(&pfd, 1, 500);
        if (ready <= 0 || (pfd.revents & POLLIN) == 0) {
            continue;  // timeout / EINTR / nothing incoming -> re-check the flag
        }

        SocketWrapper client_sock(
            accept(listen_sock_.getFd(), nullptr, nullptr));
        if (!client_sock) {
            std::cerr << "Error accepting client" << std::endl;
            continue;
        }

        auto session =
            std::make_shared<ClientSession>(std::move(client_sock), inbox_);
        dispatcher_.registerSession(session);

        {
            std::lock_guard lk(readers_mutex_);
            ++active_readers_;
        }
        // Reader holds a strong ref (keeps the session alive during recv) and
        // deregisters itself on exit so shutdown can wait for it.
        std::jthread reader([this, session] {
            session->readLoop();
            std::lock_guard lk(readers_mutex_);
            if (--active_readers_ == 0) {
                readers_cv_.notify_all();
            }
        });
        reader.detach();
    }

    std::cout << "\nshutting down..." << std::endl;

    // 1. Stop the dispatcher thread.
    dispatch_thread.request_stop();
    dispatch_thread.join();

    // 2. Wake every client's reader by shutting down its socket.
    dispatcher_.shutdownConnections();

    // 3. Wait until every reader thread has finished — only then is it safe to
    //    let inbox_ / the sessions be destroyed (readers touch inbox_ on exit).
    {
        std::unique_lock lk(readers_mutex_);
        readers_cv_.wait(lk, [this] { return active_readers_ == 0; });
    }

    std::cout << "shutdown complete" << std::endl;
    // 4. Server destruction (RAII) drops the connection table, joining each
    //    session's writer thread, and closes the listen socket.
}
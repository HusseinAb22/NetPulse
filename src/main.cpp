#include <cstdint>
#include <exception>
#include <iostream>

#include "server.h"

int main() {
    try {
        Server server(9000);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
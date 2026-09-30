#include <chrono>
#include <iostream>
#include <thread>
#include "../server-networking/ServerNetwork.hpp"

int main() {
    dustwire::server::ServerNetwork server(4674);
    if (!server.Start()) {
        std::cerr << "Unable to start server. Is UDP port 4674 already in use?\n";
        return 1;
    }

    std::cout << "DustwireMP Server v0.1. Press Ctrl+C to stop.\n";
    while (true) {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

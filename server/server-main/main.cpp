#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "../server-networking/ServerNetwork.hpp"

namespace {
volatile std::sig_atomic_t g_running = 1;

void OnSignal(int) {
    g_running = 0;
}
}

int main() {
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    dustwire::server::ServerNetwork server(4674, 32);
    if (!server.Start()) {
        std::cerr << "Unable to start DustwireMP server on UDP/ENet port 4674.\n";
        return 1;
    }

    std::cout << "DustwireMP Server v0.2. Press Ctrl+C to stop.\n";

    while (g_running) {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    server.Stop();
    std::cout << "Server stopped cleanly.\n";
    return 0;
}

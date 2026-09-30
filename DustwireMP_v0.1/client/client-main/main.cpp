#include <chrono>
#include <iostream>
#include <thread>
#include "../client-networking/ClientNetwork.hpp"
#include "../client-game/GameBridge.hpp"
#include "../client-cef/CEFBridge.hpp"

int main() {
    using namespace dustwire;

    client::ClientNetwork network;
    game::GameBridge game;
    cef::CEFBridge cef;

    if (!game.Initialize() || !network.Start()) return 1;

    cef.SetConnectHandler([&](const std::string& host, std::uint16_t port, const std::string& player_name) {
        network.Connect(host, port, player_name);
    });

    std::cout << "DustwireMP Client v0.1\n";
    std::cout << "Type connect to connect to 127.0.0.1:4674, or quit.\n";

    std::string command;
    while (std::getline(std::cin, command)) {
        if (command == "connect") {
            cef.ConnectToServer("127.0.0.1", 4674, "Player");
        } else if (command == "quit") {
            break;
        } else if (command == "state") {
            std::cout << "state=" << static_cast<int>(network.State())
                      << " player_id=" << network.LocalPlayerId() << "\n";
            if (!network.LastError().empty()) std::cout << "error=" << network.LastError() << "\n";
        }

        for (int i = 0; i < 20; ++i) {
            network.Tick();
            game.Tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    network.Disconnect();
    return 0;
}

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "../shared/net/EnetTransport.hpp"

int main() {
    using namespace dustwire::net;

    const Endpoint server_endpoint{"127.0.0.1", 4675};

    EnetTransport server(4675, 4, 2);
    EnetTransport client(0, 1, 2);

    assert(server.Start());
    assert(client.Start());
    assert(client.Connect(server_endpoint));

    bool connected = false;

    for (int i = 0; i < 400; ++i) {
        server.Poll();
        client.Poll();

        if (client.IsConnected()) {
            connected = true;
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(5));
    }

    assert(connected);

    const std::vector<std::uint8_t> payload{0x44, 0x57, 0x4D, 0x50};

    assert(client.Send(
        server_endpoint,
        payload,
        Delivery::Reliable,
        0));

    bool echoed = false;

    for (int i = 0; i < 400 && !echoed; ++i) {
        for (const auto& datagram : server.Poll()) {
            assert(datagram.payload == payload);

            assert(server.Send(
                datagram.from,
                datagram.payload,
                Delivery::Reliable,
                0));
        }

        for (const auto& datagram : client.Poll()) {
            if (datagram.payload == payload) {
                echoed = true;
            }
        }

        if (!echoed) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(5));
        }
    }

    assert(echoed);

    client.DisconnectPeer();
    client.Poll();
    server.Poll();

    server.Stop();
    client.Stop();

    std::cout << "enet_loopback: OK\n";
    return 0;
}

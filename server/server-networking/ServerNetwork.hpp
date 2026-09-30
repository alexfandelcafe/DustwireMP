#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "../../shared/net/UdpTransport.hpp"

namespace dustwire::server {

struct ClientPeer {
    dustwire::net::Endpoint endpoint;
    std::uint16_t player_id{};
    std::string player_name;
};

class ServerNetwork {
public:
    explicit ServerNetwork(std::uint16_t port);
    bool Start();
    void Stop();
    void Tick();

private:
    std::uint16_t port_{};
    dustwire::net::UdpTransport transport_;
    std::uint16_t next_player_id_{1};
    std::unordered_map<std::string, ClientPeer> peers_;
};

}

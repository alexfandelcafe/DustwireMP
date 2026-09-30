#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "../../shared/net/EnetTransport.hpp"

namespace dustwire::server {

struct ClientPeer {
    dustwire::net::Endpoint endpoint;
    std::uint16_t player_id{};
    std::string player_name;
    bool ready{false};
};

class ServerNetwork {
public:
    explicit ServerNetwork(std::uint16_t port, std::size_t max_players = 32);
    bool Start();
    void Stop();
    void Tick();

    std::size_t PlayerCount() const { return peers_.size(); }

private:
    static std::string Key(const dustwire::net::Endpoint& endpoint);
    void HandleDatagram(const dustwire::net::Datagram& datagram);

    std::uint16_t port_{};
    std::size_t max_players_{32};
    dustwire::net::EnetTransport transport_;
    std::uint16_t next_player_id_{1};
    std::unordered_map<std::string, ClientPeer> peers_;
};

}

#pragma once
#include <memory>
#include <string>
#include "../../shared/net/UdpTransport.hpp"
#include "../../shared/protocol/Protocol.hpp"

namespace dustwire::client {

enum class ConnectionState {
    Offline,
    Connecting,
    Handshaking,
    Connected,
    Disconnecting,
    Error
};

class ClientNetwork {
public:
    bool Start();
    bool Connect(const std::string& host, std::uint16_t port, const std::string& player_name);
    void Tick();
    void Disconnect();

    ConnectionState State() const { return state_; }
    std::uint16_t LocalPlayerId() const { return player_id_; }
    const std::string& LastError() const { return last_error_; }

private:
    void Fail(std::string message);

    dustwire::net::UdpTransport transport_;
    dustwire::net::Endpoint server_;
    ConnectionState state_{ConnectionState::Offline};
    std::uint32_t sequence_{0};
    std::uint16_t player_id_{0};
    std::string last_error_;
    std::string player_name_;
};

}

#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include "../../shared/net/EnetTransport.hpp"
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
    void SendPing();

    ConnectionState State() const { return state_; }
    std::uint16_t LocalPlayerId() const { return player_id_; }
    std::int64_t PingMs() const { return ping_ms_; }
    const std::string& LastError() const { return last_error_; }

private:
    void Fail(std::string message);
    void SendHello();
    void HandlePacket(const net::Datagram& datagram);

    net::EnetTransport transport_;
    net::Endpoint server_;
    ConnectionState state_{ConnectionState::Offline};
    std::uint32_t sequence_{0};
    std::uint16_t player_id_{0};
    std::string last_error_;
    std::string player_name_;
    bool hello_sent_{false};
    std::uint32_t ping_sequence_{0};
    std::chrono::steady_clock::time_point ping_started_{};
    std::int64_t ping_ms_{-1};
};

}

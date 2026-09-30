#include "ClientNetwork.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <utility>

#include "../../shared/protocol/Codec.hpp"

using namespace dustwire::protocol;

namespace dustwire::client {

bool ClientNetwork::Start() {
    if (!transport_.Start()) {
        Fail("failed to start ENet transport");
        return false;
    }

    state_ = ConnectionState::Offline;
    return true;
}

bool ClientNetwork::Connect(
    const std::string& host,
    std::uint16_t port,
    const std::string& player_name) {

    if (state_ != ConnectionState::Offline &&
        state_ != ConnectionState::Error) {
        return false;
    }

    server_ = {host, port};
    player_name_ = player_name;
    player_id_ = 0;
    ping_ms_ = -1;
    hello_sent_ = false;
    last_error_.clear();
    state_ = ConnectionState::Connecting;

    if (!transport_.Connect(server_)) {
        Fail("could not create ENet connection");
        return false;
    }

    return true;
}

void ClientNetwork::SendHello() {
    Hello hello{kProtocolVersion, "0.2.0", player_name_};

    PacketHeader header{
        kProtocolVersion,
        HELLO,
        ++sequence_
    };

    auto bytes = EncodeHeader(header);
    auto payload = EncodeHello(hello);
    bytes.insert(
        bytes.end(),
        payload.begin(),
        payload.end());

    if (!transport_.Send(
            server_,
            bytes,
            net::Delivery::Reliable,
            0)) {
        Fail("could not send HELLO");
        return;
    }

    hello_sent_ = true;
    state_ = ConnectionState::Handshaking;
}

void ClientNetwork::HandlePacket(
    const net::Datagram& datagram) {

    Reader r(
        datagram.payload.data(),
        datagram.payload.size());

    PacketHeader h;
    h.version = r.U8();
    h.opcode = r.U16();
    h.sequence = r.U32();

    if (h.version != kProtocolVersion) {
        Fail("protocol version mismatch");
        return;
    }

    constexpr std::size_t header_size = 1 + 2 + 4;

    if (h.opcode == WELCOME) {
        const auto welcome = DecodeWelcome(
            datagram.payload.data() + header_size,
            datagram.payload.size() - header_size);

        player_id_ = welcome.player_id;
        state_ = ConnectionState::Connected;

        std::cout << "Connected to "
                  << welcome.server_name
                  << " as player "
                  << player_id_
                  << "\n";

        PacketHeader ready{
            kProtocolVersion,
            READY,
            ++sequence_
        };

        if (!transport_.Send(
                server_,
                EncodeHeader(ready),
                net::Delivery::Reliable,
                0)) {
            Fail("could not send READY");
        }

        return;
    }

    if (h.opcode == PONG) {
        if (h.sequence == ping_sequence_) {
            ping_ms_ =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() -
                    ping_started_).count();

            std::cout << "PONG received: "
                      << ping_ms_
                      << " ms\n";
        }

        return;
    }
}

void ClientNetwork::Tick() {
    const auto datagrams = transport_.Poll();

    if (state_ == ConnectionState::Connecting &&
        transport_.IsConnected() &&
        !hello_sent_) {
        SendHello();
    }

    if (transport_.WasDisconnected() &&
        state_ != ConnectionState::Offline &&
        state_ != ConnectionState::Disconnecting) {
        Fail("server disconnected the ENet peer");
    }

    for (const auto& datagram : datagrams) {
        try {
            HandlePacket(datagram);
        } catch (const std::exception& e) {
            Fail(
                std::string("invalid packet: ") +
                e.what());
        }
    }
}

void ClientNetwork::SendPing() {
    if (state_ != ConnectionState::Connected) {
        return;
    }

    ping_sequence_ = ++sequence_;
    ping_started_ = std::chrono::steady_clock::now();

    PacketHeader header{
        kProtocolVersion,
        PING,
        ping_sequence_
    };

    if (!transport_.Send(
            server_,
            EncodeHeader(header),
            net::Delivery::Unreliable,
            1)) {
        Fail("could not send PING");
    }
}

void ClientNetwork::Disconnect() {
    if (state_ == ConnectionState::Offline) {
        return;
    }

    state_ = ConnectionState::Disconnecting;
    transport_.DisconnectPeer();
    transport_.Poll();

    state_ = ConnectionState::Offline;
    player_id_ = 0;
    hello_sent_ = false;
    ping_ms_ = -1;
}

void ClientNetwork::Fail(std::string message) {
    last_error_ = std::move(message);
    state_ = ConnectionState::Error;

    std::cerr << "[ClientNetwork] "
              << last_error_
              << "\n";
}

}

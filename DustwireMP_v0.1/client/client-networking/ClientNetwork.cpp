#include "ClientNetwork.hpp"
#include <iostream>
#include "../../shared/protocol/Codec.hpp"

using namespace dustwire::protocol;

namespace dustwire::client {

bool ClientNetwork::Start() {
    if (!transport_.Start()) {
        Fail("failed to start UDP transport");
        return false;
    }
    state_ = ConnectionState::Offline;
    return true;
}

bool ClientNetwork::Connect(const std::string& host, std::uint16_t port, const std::string& player_name) {
    if (state_ != ConnectionState::Offline && state_ != ConnectionState::Error) return false;
    server_ = {host, port};
    player_name_ = player_name;
    player_id_ = 0;
    last_error_.clear();
    state_ = ConnectionState::Connecting;

    PacketHeader header{kProtocolVersion, HELLO, ++sequence_};
    auto hb = EncodeHeader(header);
    Hello hello{kProtocolVersion, "0.1.0", player_name_};
    auto payload = EncodeHello(hello);
    hb.insert(hb.end(), payload.begin(), payload.end());

    if (!transport_.Send(server_, hb)) {
        Fail("could not send HELLO");
        return false;
    }
    state_ = ConnectionState::Handshaking;
    return true;
}

void ClientNetwork::Tick() {
    for (const auto& d : transport_.Poll()) {
        try {
            Reader r(d.payload.data(), d.payload.size());
            PacketHeader h;
            h.version = r.U8();
            h.opcode = r.U16();
            h.sequence = r.U32();

            if (h.version != kProtocolVersion) {
                Fail("protocol version mismatch");
                continue;
            }

            if (h.opcode == WELCOME) {
                const std::size_t consumed = 1 + 2 + 4;
                const auto welcome = DecodeWelcome(d.payload.data() + consumed, d.payload.size() - consumed);
                player_id_ = welcome.player_id;
                state_ = ConnectionState::Connected;
                std::cout << "Connected to " << welcome.server_name << " as player " << player_id_ << "\n";

                PacketHeader ready{kProtocolVersion, READY, ++sequence_};
                auto bytes = EncodeHeader(ready);
                transport_.Send(server_, bytes);
            } else if (h.opcode == PONG) {
                std::cout << "PONG received\n";
            }
        } catch (const std::exception& e) {
            Fail(std::string("invalid packet: ") + e.what());
        }
    }
}

void ClientNetwork::Disconnect() {
    transport_.Stop();
    state_ = ConnectionState::Offline;
}

void ClientNetwork::Fail(std::string message) {
    last_error_ = std::move(message);
    state_ = ConnectionState::Error;
    std::cerr << "[ClientNetwork] " << last_error_ << "\n";
}

}

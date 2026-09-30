#include "ServerNetwork.hpp"

#include <exception>
#include <iostream>
#include <utility>

#include "../../shared/protocol/Codec.hpp"

using namespace dustwire::protocol;

namespace dustwire::server {

ServerNetwork::ServerNetwork(std::uint16_t port, std::size_t max_players)
    : port_(port),
      max_players_(max_players),
      transport_(port, max_players_, 2) {}

std::string ServerNetwork::Key(const dustwire::net::Endpoint& endpoint) {
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

bool ServerNetwork::Start() {
    if (!transport_.Start()) {
        std::cerr << "[ServerNetwork] failed to start ENet on port "
                  << port_ << "\n";
        return false;
    }

    std::cout << "[ServerNetwork] ENet listening on 0.0.0.0:"
              << port_ << " max_players=" << max_players_ << "\n";
    return true;
}

void ServerNetwork::Stop() {
    transport_.Stop();
    peers_.clear();
}

void ServerNetwork::HandleDatagram(const dustwire::net::Datagram& datagram) {
    Reader r(datagram.payload.data(), datagram.payload.size());
    PacketHeader h{r.U8(), r.U16(), r.U32()};

    if (h.version != kProtocolVersion) {
        std::cerr << "[Server] protocol mismatch from "
                  << datagram.from.host << ":" << datagram.from.port << "\n";
        return;
    }

    constexpr std::size_t header_size = 1 + 2 + 4;

    if (h.opcode == HELLO) {
        const auto hello = DecodeHello(
            datagram.payload.data() + header_size,
            datagram.payload.size() - header_size);

        const std::string key = Key(datagram.from);
        auto it = peers_.find(key);

        if (it == peers_.end()) {
            if (peers_.size() >= max_players_) {
                std::cerr << "[Server] rejecting " << hello.player_name
                          << ": server full\n";
                return;
            }

            ClientPeer peer{
                datagram.from,
                next_player_id_++,
                hello.player_name,
                false
            };
            it = peers_.emplace(key, std::move(peer)).first;
        } else {
            it->second.player_name = hello.player_name;
        }

        Welcome welcome{it->second.player_id, "DustwireMP ENet Test Server"};
        PacketHeader wh{kProtocolVersion, WELCOME, h.sequence};
        auto out = EncodeHeader(wh);
        auto body = EncodeWelcome(welcome);
        out.insert(out.end(), body.begin(), body.end());

        if (!transport_.Send(
                datagram.from,
                out,
                net::Delivery::Reliable,
                0)) {
            std::cerr << "[Server] failed to send WELCOME to "
                      << datagram.from.host << ":" << datagram.from.port << "\n";
        }

        std::cout << "[Server] HELLO from " << hello.player_name
                  << " -> player_id=" << it->second.player_id << "\n";
        return;
    }

    if (h.opcode == READY) {
        const std::string key = Key(datagram.from);
        auto it = peers_.find(key);
        if (it != peers_.end()) {
            it->second.ready = true;
            std::cout << "[Server] READY player_id="
                      << it->second.player_id << "\n";
        }
        return;
    }

    if (h.opcode == PING) {
        PacketHeader ph{kProtocolVersion, PONG, h.sequence};
        transport_.Send(
            datagram.from,
            EncodeHeader(ph),
            net::Delivery::Unreliable,
            1);
        return;
    }
}

void ServerNetwork::Tick() {
    const auto datagrams = transport_.Poll();

    for (const auto& disconnected : transport_.TakeDisconnectedPeers()) {
        const auto key = Key(disconnected);
        const auto it = peers_.find(key);
        if (it != peers_.end()) {
            std::cout << "[Server] player_id=" << it->second.player_id
                      << " disconnected\n";
            peers_.erase(it);
        }
    }

    for (const auto& datagram : datagrams) {
        try {
            HandleDatagram(datagram);
        } catch (const std::exception& e) {
            std::cerr << "[Server] invalid packet: " << e.what() << "\n";
        }
    }
}

}

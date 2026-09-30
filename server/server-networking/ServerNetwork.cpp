#include "ServerNetwork.hpp"
#include <iostream>
#include "../../shared/protocol/Codec.hpp"

using namespace dustwire::protocol;

namespace dustwire::server {

ServerNetwork::ServerNetwork(std::uint16_t port) : port_(port), transport_(port) {}

bool ServerNetwork::Start() {
    if (!transport_.Start()) return false;
    std::cout << "[ServerNetwork] listening on 0.0.0.0:" << port_ << "\n";
    return true;
}

void ServerNetwork::Stop() { transport_.Stop(); }

void ServerNetwork::Tick() {
    for (const auto& d : transport_.Poll()) {
        try {
            Reader r(d.payload.data(), d.payload.size());
            PacketHeader h{r.U8(), r.U16(), r.U32()};
            const std::size_t header_size = 1 + 2 + 4;
            if (h.version != kProtocolVersion) continue;

            if (h.opcode == HELLO) {
                const auto hello = DecodeHello(d.payload.data() + header_size, d.payload.size() - header_size);
                const std::string key = d.from.host + ":" + std::to_string(d.from.port);
                ClientPeer peer{d.from, next_player_id_++, hello.player_name};
                peers_[key] = peer;

                Welcome welcome{peer.player_id, "DustwireMP Test Server"};
                PacketHeader wh{kProtocolVersion, WELCOME, h.sequence};
                auto out = EncodeHeader(wh);
                auto body = EncodeWelcome(welcome);
                out.insert(out.end(), body.begin(), body.end());
                transport_.Send(d.from, out);

                std::cout << "[Server] HELLO from " << hello.player_name
                          << " -> player_id=" << peer.player_id << "\n";
            } else if (h.opcode == READY) {
                std::cout << "[Server] client READY from " << d.from.host << ":" << d.from.port << "\n";
            } else if (h.opcode == PING) {
                PacketHeader ph{kProtocolVersion, PONG, h.sequence};
                transport_.Send(d.from, EncodeHeader(ph));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Server] invalid packet: " << e.what() << "\n";
        }
    }
}

}

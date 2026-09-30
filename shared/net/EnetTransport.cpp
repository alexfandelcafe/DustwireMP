#include "EnetTransport.hpp"

#include <enet/enet.h>
#include <utility>

namespace dustwire::net {

EnetTransport::EnetTransport(
    std::uint16_t local_port,
    std::size_t max_peers,
    std::size_t channel_count)
    : local_port_(local_port),
      max_peers_(max_peers),
      channel_count_(channel_count == 0 ? 1 : channel_count) {}

EnetTransport::~EnetTransport() {
    Stop();
}

bool EnetTransport::InitializeEnet() {
    static bool initialized = false;
    if (initialized) {
        return true;
    }

    if (enet_initialize() != 0) {
        return false;
    }

    initialized = true;
    return true;
}

bool EnetTransport::Start() {
    if (host_ != nullptr) {
        return true;
    }

    if (!InitializeEnet()) {
        return false;
    }

    if (local_port_ == 0) {
        host_ = enet_host_create(
            nullptr,
            max_peers_,
            channel_count_,
            0,
            0);
    } else {
        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = local_port_;

        host_ = enet_host_create(
            &address,
            max_peers_,
            channel_count_,
            0,
            0);
    }

    return host_ != nullptr;
}

void EnetTransport::DisconnectPeer() {
    if (connected_peer_ != nullptr) {
        enet_peer_disconnect(connected_peer_, 0);
    }
}

void EnetTransport::Stop() {
    if (host_ == nullptr) {
        return;
    }

    if (connected_peer_ != nullptr) {
        enet_peer_disconnect_now(connected_peer_, 0);
        connected_peer_ = nullptr;
    }

    for (const auto& [key, peer] : peers_) {
        (void)key;
        if (peer != nullptr && peer != connected_peer_) {
            enet_peer_disconnect_now(peer, 0);
        }
    }

    enet_host_flush(host_);
    enet_host_destroy(host_);
    host_ = nullptr;
    peers_.clear();
}

std::string EnetTransport::EndpointKey(const Endpoint& endpoint) {
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

Endpoint EnetTransport::EndpointFromAddress(const ENetAddress& address) {
    char host[64]{};

    if (enet_address_get_host_ip(
            &address,
            host,
            sizeof(host)) != 0) {
        return {};
    }

    return Endpoint{host, address.port};
}

bool EnetTransport::Connect(const Endpoint& endpoint) {
    if (host_ == nullptr || connected_peer_ != nullptr) {
        return false;
    }

    ENetAddress address{};

    if (enet_address_set_host(
            &address,
            endpoint.host.c_str()) != 0) {
        return false;
    }

    address.port = endpoint.port;

    connected_peer_ = enet_host_connect(
        host_,
        &address,
        channel_count_,
        0);

    return connected_peer_ != nullptr;
}

bool EnetTransport::SendToPeer(
    _ENetPeer* peer,
    const std::vector<std::uint8_t>& data,
    Delivery delivery,
    std::uint8_t channel) {

    if (peer == nullptr ||
        host_ == nullptr ||
        channel >= channel_count_) {
        return false;
    }

    const enet_uint32 flags =
        delivery == Delivery::Reliable
            ? ENET_PACKET_FLAG_RELIABLE
            : 0;

    ENetPacket* packet = enet_packet_create(
        data.data(),
        data.size(),
        flags);

    if (packet == nullptr) {
        return false;
    }

    if (enet_peer_send(
            peer,
            channel,
            packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }

    return true;
}

bool EnetTransport::Send(
    const Endpoint& endpoint,
    const std::vector<std::uint8_t>& data) {

    return Send(
        endpoint,
        data,
        Delivery::Reliable,
        0);
}

bool EnetTransport::Send(
    const Endpoint& endpoint,
    const std::vector<std::uint8_t>& data,
    Delivery delivery,
    std::uint8_t channel) {

    if (host_ == nullptr) {
        return false;
    }

    _ENetPeer* peer = nullptr;

    if (connected_peer_ != nullptr &&
        max_peers_ == 1) {
        peer = connected_peer_;
    } else {
        const auto it =
            peers_.find(EndpointKey(endpoint));

        if (it != peers_.end()) {
            peer = it->second;
        }
    }

    return SendToPeer(
        peer,
        data,
        delivery,
        channel);
}

std::vector<Datagram> EnetTransport::Poll() {
    std::vector<Datagram> out;

    if (host_ == nullptr) {
        return out;
    }

    ENetEvent event{};

    while (enet_host_service(
               host_,
               &event,
               0) > 0) {

        if (event.type ==
            ENET_EVENT_TYPE_CONNECT) {

            const Endpoint endpoint =
                EndpointFromAddress(event.peer->address);

            peers_[EndpointKey(endpoint)] =
                event.peer;

            if (max_peers_ == 1) {
                connected_peer_ = event.peer;
            }

            continue;
        }

        if (event.type ==
            ENET_EVENT_TYPE_DISCONNECT) {

            const Endpoint endpoint =
                EndpointFromAddress(event.peer->address);

            peers_.erase(EndpointKey(endpoint));

            if (event.peer == connected_peer_) {
                connected_peer_ = nullptr;
                disconnected_flag_ = true;
            }

            disconnected_peers_.push_back(endpoint);
            continue;
        }

        if (event.type ==
            ENET_EVENT_TYPE_RECEIVE &&
            event.packet != nullptr) {

            Datagram datagram;

            datagram.from =
                EndpointFromAddress(
                    event.peer->address);

            datagram.payload.assign(
                event.packet->data,
                event.packet->data +
                event.packet->dataLength);

            out.push_back(
                std::move(datagram));

            enet_packet_destroy(
                event.packet);
        }
    }

    return out;
}

bool EnetTransport::IsConnected() const {
    return connected_peer_ != nullptr &&
           connected_peer_->state ==
               ENET_PEER_STATE_CONNECTED;
}

bool EnetTransport::WasDisconnected() {
    const bool value =
        disconnected_flag_;

    disconnected_flag_ = false;
    return value;
}

std::vector<Endpoint>
EnetTransport::TakeDisconnectedPeers() {
    std::vector<Endpoint> result;
    result.swap(disconnected_peers_);
    return result;
}

}

#pragma once
#include "Transport.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace dustwire::net {

enum class Delivery : std::uint8_t {
    Unreliable = 0,
    Reliable = 1,
};

class EnetTransport final : public ITransport {
public:
    explicit EnetTransport(std::uint16_t local_port = 0, std::size_t max_peers = 1, std::size_t channel_count = 2);
    ~EnetTransport() override;

    bool Start() override;
    void Stop() override;
    bool Send(const Endpoint&, const std::vector<std::uint8_t>&) override;
    bool Send(const Endpoint&, const std::vector<std::uint8_t>&, Delivery, std::uint8_t channel);
    std::vector<Datagram> Poll() override;

    bool Connect(const Endpoint& endpoint);
    bool IsConnected() const;
    bool WasDisconnected();
    std::vector<Endpoint> TakeDisconnectedPeers();

private:
    bool InitializeEnet();
    bool SendToPeer(_ENetPeer* peer, const std::vector<std::uint8_t>& data, Delivery delivery, std::uint8_t channel);
    static std::string EndpointKey(const Endpoint& endpoint);
    static Endpoint EndpointFromAddress(const struct _ENetAddress& address);

    std::uint16_t local_port_{};
    std::size_t max_peers_{1};
    std::size_t channel_count_{2};
    _ENetHost* host_{nullptr};
    _ENetPeer* connected_peer_{nullptr};
    std::unordered_map<std::string, _ENetPeer*> peers_;
    std::vector<Endpoint> disconnected_peers_;
    bool disconnected_flag_{false};
};

}

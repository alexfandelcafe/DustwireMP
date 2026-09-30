#pragma once
#include "Transport.hpp"
#include <cstdint>

namespace dustwire::net {

class UdpTransport final : public ITransport {
public:
    explicit UdpTransport(std::uint16_t local_port = 0);
    ~UdpTransport() override;

    bool Start() override;
    void Stop() override;
    bool Send(const Endpoint&, const std::vector<std::uint8_t>&) override;
    std::vector<Datagram> Poll() override;

private:
    std::uint16_t local_port_{};
    std::uintptr_t socket_{static_cast<std::uintptr_t>(-1)};
};

}

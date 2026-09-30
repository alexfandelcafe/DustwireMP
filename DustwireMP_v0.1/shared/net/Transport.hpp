#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dustwire::net {

struct Endpoint {
    std::string host;
    std::uint16_t port{};
};

struct Datagram {
    Endpoint from;
    std::vector<std::uint8_t> payload;
};

class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool Send(const Endpoint&, const std::vector<std::uint8_t>&) = 0;
    virtual std::vector<Datagram> Poll() = 0;
};

}

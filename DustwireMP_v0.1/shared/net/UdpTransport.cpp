#include "UdpTransport.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>

namespace dustwire::net {

UdpTransport::UdpTransport(std::uint16_t local_port) : local_port_(local_port) {}
UdpTransport::~UdpTransport() { Stop(); }

bool UdpTransport::Start() {
    static bool wsa_started = false;
    if (!wsa_started) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        wsa_started = true;
    }
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;

    if (local_port_ != 0) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(local_port_);
        if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            return false;
        }
    }

    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
    socket_ = static_cast<std::uintptr_t>(s);
    return true;
}

void UdpTransport::Stop() {
    if (socket_ != static_cast<std::uintptr_t>(-1)) {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = static_cast<std::uintptr_t>(-1);
    }
}

bool UdpTransport::Send(const Endpoint& endpoint, const std::vector<std::uint8_t>& data) {
    if (socket_ == static_cast<std::uintptr_t>(-1)) return false;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.host.c_str(), &dest.sin_addr) != 1) return false;
    const auto sent = sendto(static_cast<SOCKET>(socket_), reinterpret_cast<const char*>(data.data()),
                             static_cast<int>(data.size()), 0,
                             reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    return sent == static_cast<int>(data.size());
}

std::vector<Datagram> UdpTransport::Poll() {
    std::vector<Datagram> out;
    if (socket_ == static_cast<std::uintptr_t>(-1)) return out;
    std::uint8_t buffer[65535];
    for (;;) {
        sockaddr_in from{};
        int from_len = sizeof(from);
        const int n = recvfrom(static_cast<SOCKET>(socket_), reinterpret_cast<char*>(buffer), sizeof(buffer),
                               0, reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n == SOCKET_ERROR) break;
        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
        Datagram d;
        d.from.host = ip;
        d.from.port = ntohs(from.sin_port);
        d.payload.assign(buffer, buffer + n);
        out.push_back(std::move(d));
    }
    return out;
}

}

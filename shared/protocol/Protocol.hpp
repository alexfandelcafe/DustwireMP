#pragma once
#include <cstdint>
#include <string>

namespace dustwire::protocol {

constexpr std::uint8_t kProtocolVersion = 1;

inline constexpr std::uint16_t HELLO = 0x0001;
inline constexpr std::uint16_t WELCOME = 0x0002;
inline constexpr std::uint16_t READY = 0x0003;
inline constexpr std::uint16_t PING = 0x0004;
inline constexpr std::uint16_t PONG = 0x0005;

struct PacketHeader {
    std::uint8_t version{};
    std::uint16_t opcode{};
    std::uint32_t sequence{};
};

struct Hello {
    std::uint8_t protocol_version{};
    std::string client_version;
    std::string player_name;
};

struct Welcome {
    std::uint16_t player_id{};
    std::string server_name;
};

}

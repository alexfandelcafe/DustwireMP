#pragma once
#include "ByteBuffer.hpp"
#include "Protocol.hpp"

namespace dustwire::protocol {

inline std::vector<std::uint8_t> EncodeHello(const Hello& h) {
    Writer w;
    w.U8(h.protocol_version);
    w.String(h.client_version);
    w.String(h.player_name);
    return w.Data();
}

inline Hello DecodeHello(const std::uint8_t* data, std::size_t size) {
    Reader r(data, size);
    Hello h;
    h.protocol_version = r.U8();
    h.client_version = r.String();
    h.player_name = r.String();
    return h;
}

inline std::vector<std::uint8_t> EncodeWelcome(const Welcome& wv) {
    Writer w;
    w.U16(wv.player_id);
    w.String(wv.server_name);
    return w.Data();
}

inline Welcome DecodeWelcome(const std::uint8_t* data, std::size_t size) {
    Reader r(data, size);
    Welcome w;
    w.player_id = r.U16();
    w.server_name = r.String();
    return w;
}

inline std::vector<std::uint8_t> EncodeHeader(const PacketHeader& h) {
    Writer w;
    w.U8(h.version);
    w.U16(h.opcode);
    w.U32(h.sequence);
    return w.Data();
}

}

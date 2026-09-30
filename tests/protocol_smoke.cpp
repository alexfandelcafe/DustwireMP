#include <cassert>
#include <iostream>
#include "../../shared/protocol/Codec.hpp"

int main() {
    using namespace dustwire::protocol;

    Hello hello{kProtocolVersion, "0.2.0", "DustwireTester"};
    const auto hello_bytes = EncodeHello(hello);
    const auto decoded_hello = DecodeHello(hello_bytes.data(), hello_bytes.size());

    assert(decoded_hello.protocol_version == hello.protocol_version);
    assert(decoded_hello.client_version == hello.client_version);
    assert(decoded_hello.player_name == hello.player_name);

    Welcome welcome{42, "Dustwire ENet Test"};
    const auto welcome_bytes = EncodeWelcome(welcome);
    const auto decoded_welcome = DecodeWelcome(welcome_bytes.data(), welcome_bytes.size());

    assert(decoded_welcome.player_id == welcome.player_id);
    assert(decoded_welcome.server_name == welcome.server_name);

    PacketHeader header{kProtocolVersion, HELLO, 1234};
    const auto header_bytes = EncodeHeader(header);
    Reader reader(header_bytes.data(), header_bytes.size());

    assert(reader.U8() == header.version);
    assert(reader.U16() == header.opcode);
    assert(reader.U32() == header.sequence);

    std::cout << "protocol_smoke: OK\n";
    return 0;
}

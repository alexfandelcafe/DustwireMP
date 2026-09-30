#include "CEFBridge.hpp"
#include <iostream>

namespace dustwire::cef {

void CEFBridge::ConnectToServer(const std::string& host, std::uint16_t port, const std::string& player_name) const {
    std::cout << "[CEFBridge] JS requested connection to " << host << ":" << port << "\n";
    if (connect_handler_) connect_handler_(host, port, player_name);
}

const char* CEFBridge::GetConnectionStateJson() const {
    return "{\"state\":\"standalone\"}";
}

}

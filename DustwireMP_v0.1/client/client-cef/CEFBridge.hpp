#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace dustwire::cef {

class CEFBridge {
public:
    using ConnectFn = std::function<void(const std::string&, std::uint16_t, const std::string&)>;

    void SetConnectHandler(ConnectFn handler) { connect_handler_ = std::move(handler); }

    // These are the native methods exposed to the future CEF browser process.
    void ConnectToServer(const std::string& host, std::uint16_t port, const std::string& player_name) const;
    const char* GetConnectionStateJson() const;

private:
    ConnectFn connect_handler_;
};

}

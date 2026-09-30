#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace dustwire::client {

class IGameTickSource {
public:
    virtual ~IGameTickSource() = default;
    virtual bool Start(std::function<void()> callback) = 0;
    virtual void Stop() = 0;
};

class BootstrapTickSource final : public IGameTickSource {
public:
    explicit BootstrapTickSource(
        std::chrono::milliseconds interval =
            std::chrono::milliseconds(16))
        : interval_(interval) {}

    ~BootstrapTickSource() override {
        Stop();
    }

    bool Start(std::function<void()> callback) override;
    void Stop() override;

private:
    std::chrono::milliseconds interval_;
    std::atomic_bool running_{false};
    std::thread worker_;
};

}

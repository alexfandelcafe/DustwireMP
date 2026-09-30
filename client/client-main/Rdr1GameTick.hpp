#pragma once

#include "GameTick.hpp"

#include <atomic>
#include <functional>
#include <thread>

#include "../client-game/rdr1/GameThreadDispatcher.hpp"
#include "../client-game/rdr1/NativeInvoker.hpp"

namespace dustwire::client {

class Rdr1GameTickSource final : public IGameTickSource {
public:
    ~Rdr1GameTickSource() override {
        Stop();
    }

    bool Start(std::function<void()> callback) override;
    void Stop() override;

private:
    bool InitializeRdr1();
    void Bootstrap();

    std::atomic_bool running_{false};
    std::atomic_bool ready_{false};
    std::thread worker_;

    std::function<void()> callback_;

    rdr1::NativeInvoker native_invoker_;
    rdr1::GameThreadDispatcher dispatcher_;
};

}

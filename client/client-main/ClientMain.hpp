#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>

#include "../client-game/GameBridge.hpp"
#include "../client-networking/ClientNetwork.hpp"
#include "Rdr1GameTick.hpp"

namespace dustwire::client {

class ClientMain {
public:
    bool Initialize(HMODULE module);
    void Stop();

    bool IsInitialized() const {
        return initialized_.load();
    }

private:
    void Tick();

    std::atomic_bool initialized_{false};
    std::atomic_bool stopping_{false};

    std::filesystem::path module_directory_;

    ClientNetwork network_;
    game::GameBridge game_;
    Rdr1GameTickSource tick_source_;
    std::uint64_t tick_count_{0};
};

}

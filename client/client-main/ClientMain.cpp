#include "ClientMain.hpp"

#include <filesystem>
#include <sstream>

#include "../../shared/logging/Logger.hpp"

namespace dustwire::client {

bool ClientMain::Initialize(HMODULE module) {
    if (initialized_.exchange(true)) {
        return true;
    }

    wchar_t module_path[MAX_PATH * 4]{};

    const DWORD length =
        GetModuleFileNameW(
            module,
            module_path,
            static_cast<DWORD>(
                std::size(module_path)));

    if (length == 0 ||
        length >= std::size(module_path)) {
        initialized_ = false;
        return false;
    }

    module_directory_ =
        std::filesystem::path(module_path).parent_path();

    logging::Logger::Instance().Initialize(
        module_directory_ / "logs",
        "client-main");

    auto& logger =
        logging::Logger::Instance();

    logger.Info(
        "client-main.dll bootstrap started");

    if (!game_.Initialize()) {
        logger.Error(
            "GameBridge initialization failed");
        initialized_ = false;
        return false;
    }

    if (!network_.Start()) {
        logger.Error(
            "ClientNetwork initialization failed");
        initialized_ = false;
        return false;
    }

    stopping_ = false;

    if (!tick_source_.Start([this]() {
            Tick();
        })) {

        logger.Error(
            "failed to start bootstrap tick source");

        network_.Disconnect();
        initialized_ = false;
        return false;
    }

    std::ostringstream message;
    message << "client-main initialized from "
            << module_directory_.string();

    logger.Info(message.str());
    return true;
}

void ClientMain::Stop() {
    if (!initialized_.load() ||
        stopping_.exchange(true)) {
        return;
    }

    logging::Logger::Instance().Info(
        "client-main shutdown requested");

    tick_source_.Stop();
    network_.Disconnect();

    initialized_ = false;
}

void ClientMain::Tick() {
    if (stopping_.load()) {
        return;
    }

    network_.Tick();
    game_.Tick();
}

}

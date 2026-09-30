#include <Windows.h>

#include <filesystem>
#include <sstream>

#include "../../shared/logging/Logger.hpp"
#include "GameBuild.hpp"
#include "Injector.hpp"
#include "LauncherConfig.hpp"
#include "ProcessLocator.hpp"

namespace {

std::filesystem::path ExecutableDirectory() {
    wchar_t path[MAX_PATH * 4]{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            path,
            static_cast<DWORD>(
                sizeof(path) / sizeof(path[0])));

    if (length == 0 ||
        length >=
            sizeof(path) / sizeof(path[0])) {

        return std::filesystem::current_path();
    }

    return std::filesystem::path(path)
        .parent_path();
}

std::filesystem::path Resolve(
    const std::filesystem::path& base,
    const std::filesystem::path& path) {

    if (path.is_absolute()) {
        return path;
    }

    return base / path;
}

std::string Narrow(
    const std::wstring& value) {

    if (value.empty()) {
        return {};
    }

    const int length =
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(
                value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

    if (length <= 0) {
        return {};
    }

    std::string result(
        static_cast<std::size_t>(
            length),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(
            value.size()),
        result.data(),
        length,
        nullptr,
        nullptr);

    return result;
}

}

int wmain() {
    using namespace dustwire::launcher;

    const auto root =
        ExecutableDirectory();

    dustwire::logging::Logger::Instance()
        .Initialize(
            root / "logs",
            "launcher");

    auto& logger =
        dustwire::logging::Logger::Instance();

    logger.Info(
        "DustwireMPLauncher v0.3 starting");

    const auto config =
        LauncherConfigReader().Load(
            root /
            "config" /
            "launcher.ini");

    const auto game_path =
        Resolve(
            root,
            config.game_path);

    const auto client_dll =
        Resolve(
            root,
            config.client_dll);

    logger.Info(
        "Launcher root: " +
        root.string());

    logger.Info(
        "Game path: " +
        game_path.string());

    logger.Info(
        "Client DLL: " +
        client_dll.string());

    ProcessLocator locator;

    auto game_process =
        locator.FindFirstByName(
            config.target_process);

    if (game_process.pid == 0) {
        logger.Info(
            "RDR.exe is not running; launching it");

        if (!std::filesystem::exists(
                game_path)) {

            logger.Error(
                "configured game executable does not exist");
            return 2;
        }

        std::uint32_t launched_pid = 0;

        if (!locator.Launch(
                game_path.wstring(),
                game_path.parent_path().wstring(),
                launched_pid)) {

            logger.Error(
                "failed to launch RDR.exe");
            return 3;
        }

        std::ostringstream message;
        message << "RDR.exe launched with pid="
                << launched_pid;

        logger.Info(message.str());

        if (!locator.WaitForProcess(
                config.target_process,
                config.wait_for_game_ms,
                game_process)) {

            logger.Error(
                "timed out waiting for RDR.exe");
            return 4;
        }
    }

    std::ostringstream target_message;
    target_message << "Target pid="
                   << game_process.pid
                   << " image="
                   << Narrow(game_process.image_path);

    logger.Info(target_message.str());

    GameBuild build;

    const auto build_info =
        build.Inspect(
            game_process.image_path);

    if (!build_info.valid_pe) {
        logger.Error(
            "target executable has an invalid PE image");
        return 5;
    }

    std::ostringstream fingerprint;
    fingerprint << "PE machine=0x"
                << std::hex
                << build_info.machine
                << " x64="
                << std::dec
                << (build_info.is_64_bit ? 1 : 0)
                << " timestamp="
                << build_info.timestamp
                << " image_size=0x"
                << std::hex
                << build_info.image_size
                << " file_size="
                << std::dec
                << build_info.file_size;

    logger.Info(
        fingerprint.str());

    if (config.require_x64) {
        std::wstring reason;

        if (!build.IsSupported(
                build_info,
                reason)) {

            logger.Error(
                "unsupported game build: " +
                Narrow(reason));
            return 6;
        }
    }

    if (!std::filesystem::exists(
            client_dll)) {

        logger.Error(
            "client-main DLL not found: " +
            client_dll.string());

        return 7;
    }

    logger.Info(
        "injecting DustwireMPClientModule.dll");

    const auto result =
        Injector().Inject(
            game_process.pid,
            client_dll.wstring());

    if (!result.success) {
        logger.Error(
            "DLL injection failed: " +
            result.error);
        return 8;
    }

    if (result.already_loaded) {
        logger.Warning(
            "client-main DLL already loaded; "
            "injection skipped");
    } else {
        std::ostringstream message;
        message << "DLL injection succeeded; "
                << "remote module handle=0x"
                << std::hex
                << result.remote_exit_code;

        logger.Info(
            message.str());
    }

    logger.Info(
        "DustwireMPLauncher finished successfully");

    return 0;
}

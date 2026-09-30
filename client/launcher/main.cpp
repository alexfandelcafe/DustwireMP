#include <Windows.h>

#include <filesystem>
#include <iomanip>
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
                sizeof(path) /
                sizeof(path[0])));

    if (length == 0 ||
        length >= sizeof(path) / sizeof(path[0])) {
        return std::filesystem::current_path();
    }

    return std::filesystem::path(path).parent_path();
}

std::filesystem::path LogDirectory(
    const std::filesystem::path& executableDirectory) {

    const auto workingDirectory =
        std::filesystem::current_path();

    if (std::filesystem::exists(
            workingDirectory / "build-vs2026")) {
        return workingDirectory / "logs";
    }

    return executableDirectory / "logs";
}

std::filesystem::path Resolve(
    const std::filesystem::path& base,
    const std::filesystem::path& path) {

    return path.is_absolute()
        ? path
        : base / path;
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
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

    if (length <= 0) {
        return {};
    }

    std::string result(
        static_cast<std::size_t>(length),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length,
        nullptr,
        nullptr);

    return result;
}

bool MatchesExpectedProfile(
    const dustwire::launcher::LauncherConfig& config,
    const dustwire::launcher::GameBuildInfo& build) {

    if (config.expected_machine != 0 &&
        config.expected_machine != build.machine) {
        return false;
    }

    if (config.expected_timestamp != 0 &&
        config.expected_timestamp != build.timestamp) {
        return false;
    }

    if (config.expected_image_size != 0 &&
        config.expected_image_size != build.image_size) {
        return false;
    }

    if (config.expected_file_size != 0 &&
        config.expected_file_size != build.file_size) {
        return false;
    }

    if (config.expected_text_rva != 0 &&
        config.expected_text_rva != build.text_rva) {
        return false;
    }

    if (config.expected_text_size != 0 &&
        config.expected_text_size != build.text_size) {
        return false;
    }

    if (config.expected_text_fnv1a64 != 0 &&
        config.expected_text_fnv1a64 != build.text_fnv1a64) {
        return false;
    }

    if (!config.expected_file_version.empty() &&
        config.expected_file_version != build.file_version) {
        return false;
    }

    if (!config.expected_sha256.empty() &&
        config.expected_sha256 != build.sha256) {
        return false;
    }

    return true;
}

}

int wmain(int argc, wchar_t** argv) {
    using namespace dustwire::launcher;

    const auto root =
        ExecutableDirectory();

    const auto logDirectory =
        LogDirectory(root);

    dustwire::logging::Logger::Instance().Initialize(
        logDirectory,
        "launcher");

    auto& logger =
        dustwire::logging::Logger::Instance();

    logger.Info(
        "DustwireMPLauncher v0.3 starting");

    logger.Info(
        "Launcher log directory: " +
        logDirectory.string());

    const auto config =
        LauncherConfigReader().Load(
            root / "config" / "launcher.ini");

    std::filesystem::path configured_game_path;

    if (argc >= 2 && argv[1] != nullptr && *argv[1] != L'\\0') {
        configured_game_path = argv[1];
        logger.Info(
            "Game path supplied on command line: " +
            configured_game_path.string());
    } else if (!config.game_path.empty()) {
        configured_game_path =
            Resolve(root, config.game_path);
    } else {
        wchar_t environmentPath[32768]{};
        const DWORD environmentLength =
            GetEnvironmentVariableW(
                L"DUSTWIRE_RDR1_PATH",
                environmentPath,
                static_cast<DWORD>(
                    sizeof(environmentPath) /
                    sizeof(environmentPath[0])));

        if (environmentLength != 0 &&
            environmentLength <
                sizeof(environmentPath) /
                    sizeof(environmentPath[0])) {
            configured_game_path =
                std::filesystem::path(environmentPath);
            logger.Info(
                "Game path supplied by DUSTWIRE_RDR1_PATH: " +
                configured_game_path.string());
        }
    }

    const auto client_dll =
        Resolve(root, config.client_dll);

    logger.Info(
        "Launcher root: " +
        root.string());

    logger.Info(
        "Game path: " +
        (configured_game_path.empty()
            ? std::string("<not configured>")
            : configured_game_path.string()));

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

        if (configured_game_path.empty()) {
            logger.Error(
                "No RDR.exe path configured. Use run_launcher.bat \"C:\\path\\to\\RDR.exe\", "
                "set game_path=... in config/launcher.ini, or set DUSTWIRE_RDR1_PATH.");
            return 2;
        }

        if (!std::filesystem::exists(configured_game_path)) {
            logger.Error(
                "configured RDR.exe does not exist: " +
                configured_game_path.string());
            return 2;
        }

        std::uint32_t launched_pid = 0;

        if (!locator.Launch(
                configured_game_path.wstring(),
                configured_game_path.parent_path().wstring(),
                launched_pid)) {
            logger.Error(
                "failed to launch RDR.exe");
            return 3;
        }

        std::ostringstream launched;
        launched << "RDR.exe launched with pid="
                 << launched_pid;

        logger.Info(launched.str());

        if (!locator.WaitForProcess(
                config.target_process,
                config.wait_for_game_ms,
                game_process)) {
            logger.Error(
                "timed out waiting for RDR.exe");
            return 4;
        }
    }

    std::ostringstream target;
    target << "Target pid="
           << game_process.pid
           << " image="
           << Narrow(game_process.image_path);

    logger.Info(target.str());

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
                << " timestamp=0x"
                << std::hex
                << build_info.timestamp
                << " image_size=0x"
                << build_info.image_size
                << " file_size="
                << std::dec
                << build_info.file_size
                << " text_rva=0x"
                << std::hex
                << build_info.text_rva
                << " text_size=0x"
                << build_info.text_size
                << " text_fnv1a64=0x"
                << build_info.text_fnv1a64
                << " file_version="
                << (build_info.file_version.empty()
                    ? "<unknown>"
                    : build_info.file_version)
                << " sha256="
                << build_info.sha256;

    logger.Info(fingerprint.str());

    if (config.require_x64) {
        std::wstring reason;

        if (!build.IsSupported(
                build_info,
                reason)) {
            logger.Error(
                "unsupported architecture: " +
                Narrow(reason));
            return 6;
        }
    }

    const bool has_exact_profile =
        config.expected_machine != 0 ||
        config.expected_timestamp != 0 ||
        config.expected_image_size != 0 ||
        config.expected_file_size != 0 ||
        config.expected_text_rva != 0 ||
        config.expected_text_size != 0 ||
        config.expected_text_fnv1a64 != 0 ||
        !config.expected_file_version.empty() ||
        !config.expected_sha256.empty();

    if (has_exact_profile) {
        if (!MatchesExpectedProfile(
                config,
                build_info)) {
            logger.Error(
                "RDR.exe does not match the configured exact build profile");
            return 7;
        }

        logger.Info(
            "exact RDR1 build profile matched");
    } else {
        logger.Warning(
            "no exact RDR1 build profile configured; x64-only bootstrap check is active");
    }

    if (!std::filesystem::exists(client_dll)) {
        logger.Error(
            "client-main DLL not found: " +
            client_dll.string());
        return 8;
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
        return 9;
    }

    if (result.already_loaded) {
        logger.Warning(
            "client-main DLL already loaded; injection skipped");
    } else {
        std::ostringstream injected;

        injected
            << "DLL injection succeeded; remote module handle=0x"
            << std::hex
            << result.remote_exit_code;

        logger.Info(injected.str());
    }

    logger.Info(
        "DustwireMPLauncher finished successfully");

    return 0;
}

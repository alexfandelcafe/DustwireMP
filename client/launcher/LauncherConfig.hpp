#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace dustwire::launcher {

struct LauncherConfig {
    std::filesystem::path game_path{"RDR.exe"};
    std::filesystem::path client_dll{
        "DustwireMPClientModule.dll"
    };
    std::wstring target_process{L"RDR.exe"};
    std::uint32_t wait_for_game_ms{30000};
    bool require_x64{true};
};

class LauncherConfigReader {
public:
    LauncherConfig Load(
        const std::filesystem::path& path) const;
};

}

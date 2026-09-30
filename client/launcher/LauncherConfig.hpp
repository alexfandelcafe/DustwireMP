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
    bool inspect_only{false};

    // Zero/empty means "report only". Non-zero values enable
    // an exact build-profile check.
    std::uint32_t expected_timestamp{0};
    std::uint32_t expected_image_size{0};
    std::uint64_t expected_file_size{0};
    std::uint16_t expected_machine{0};
    std::string expected_sha256;
};

class LauncherConfigReader {
public:
    LauncherConfig Load(
        const std::filesystem::path& path) const;
};

}

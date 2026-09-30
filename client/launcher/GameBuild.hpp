#pragma once

#include <cstdint>
#include <string>

namespace dustwire::launcher {

struct GameBuildInfo {
    bool valid_pe{false};
    bool is_64_bit{false};
    std::uint16_t machine{};
    std::uint32_t timestamp{};
    std::uint32_t image_size{};
    std::uint64_t file_size{};
    std::string sha256;
    std::wstring path;
    std::wstring description;
};

class GameBuild {
public:
    GameBuildInfo Inspect(
        const std::wstring& executable_path) const;

    bool IsSupported(
        const GameBuildInfo& build,
        std::wstring& reason) const;
};

}

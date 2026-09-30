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
    std::uint32_t text_rva{};
    std::uint32_t text_size{};
    std::uint64_t text_fnv1a64{};
    std::string sha256;
    std::string file_version;
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

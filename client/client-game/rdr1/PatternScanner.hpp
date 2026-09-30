#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dustwire::rdr1 {

struct BytePattern final {
    std::vector<std::optional<std::uint8_t>> bytes;

    static std::optional<BytePattern> Parse(
        const std::string& text);
};

class PatternScanner final {
public:
    static std::optional<std::uintptr_t> ScanBuffer(
        const std::uint8_t* data,
        std::size_t size,
        const BytePattern& pattern);

    static bool ValidateInModule(
        std::uintptr_t address,
        std::uintptr_t module_base,
        std::size_t image_size);
};

}

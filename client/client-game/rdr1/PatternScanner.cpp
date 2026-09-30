#include "PatternScanner.hpp"

#include <sstream>

namespace dustwire::rdr1 {

std::optional<BytePattern> BytePattern::Parse(
    const std::string& text) {

    BytePattern result;
    std::istringstream stream(text);
    std::string token;

    while (stream >> token) {
        if (token == "?" || token == "??") {
            result.bytes.emplace_back(std::nullopt);
            continue;
        }

        if (token.size() != 2) {
            return std::nullopt;
        }

        const auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };

        const int hi = hex(token[0]);
        const int lo = hex(token[1]);

        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }

        result.bytes.emplace_back(
            static_cast<std::uint8_t>((hi << 4) | lo));
    }

    if (result.bytes.empty()) {
        return std::nullopt;
    }

    return result;
}

std::optional<std::uintptr_t> PatternScanner::ScanBuffer(
    const std::uint8_t* data,
    std::size_t size,
    const BytePattern& pattern) {

    if (data == nullptr ||
        pattern.bytes.empty() ||
        size < pattern.bytes.size()) {
        return std::nullopt;
    }

    for (std::size_t i = 0;
         i + pattern.bytes.size() <= size;
         ++i) {

        bool match = true;

        for (std::size_t j = 0;
             j < pattern.bytes.size();
             ++j) {

            if (pattern.bytes[j].has_value() &&
                data[i + j] != pattern.bytes[j].value()) {
                match = false;
                break;
            }
        }

        if (match) {
            return reinterpret_cast<std::uintptr_t>(
                data + i);
        }
    }

    return std::nullopt;
}

bool PatternScanner::ValidateInModule(
    std::uintptr_t address,
    std::uintptr_t module_base,
    std::size_t image_size) {

    if (address < module_base ||
        image_size == 0) {
        return false;
    }

    const auto end = module_base + image_size;
    return address < end;
}

}

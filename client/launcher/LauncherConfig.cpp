#include "LauncherConfig.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace dustwire::launcher {

namespace {

std::string Trim(std::string value) {
    const auto not_space =
        [](unsigned char c) {
            return !std::isspace(c);
        };

    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), not_space));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), not_space).base(),
        value.end());

    return value;
}

std::wstring Widen(const std::string& value) {
    if (value.empty()) return {};

    const int length = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) return {};

    std::wstring result(
        static_cast<std::size_t>(length), L'\0');

    MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), length);
    return result;
}

bool ParseBool(const std::string& value, bool fallback) {
    std::string normalized = value;

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    if (normalized == "true" ||
        normalized == "1" ||
        normalized == "yes") {
        return true;
    }

    if (normalized == "false" ||
        normalized == "0" ||
        normalized == "no") {
        return false;
    }

    return fallback;
}

std::uint64_t ParseU64(
    const std::string& value,
    std::uint64_t fallback) {

    try {
        std::size_t consumed = 0;
        const auto parsed =
            std::stoull(value, &consumed, 0);

        return consumed == value.size()
            ? parsed
            : fallback;
    } catch (...) {
        return fallback;
    }
}

std::uint32_t ParseU32(
    const std::string& value,
    std::uint32_t fallback) {

    const auto parsed = ParseU64(value, fallback);

    if (parsed > 0xFFFFFFFFULL) {
        return fallback;
    }

    return static_cast<std::uint32_t>(parsed);
}

std::uint16_t ParseU16(
    const std::string& value,
    std::uint16_t fallback) {

    const auto parsed = ParseU64(value, fallback);

    if (parsed > 0xFFFFULL) {
        return fallback;
    }

    return static_cast<std::uint16_t>(parsed);
}

bool IsSha256(const std::string& value) {
    if (value.size() != 64) {
        return false;
    }

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
}

std::string NormalizeSha256(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    return value;
}

}

LauncherConfig LauncherConfigReader::Load(
    const std::filesystem::path& path) const {

    LauncherConfig config;

    std::ifstream input(path);
    if (!input) {
        return config;
    }

    std::string line;

    while (std::getline(input, line)) {
        line = Trim(line);

        if (line.empty() ||
            line[0] == '#' ||
            line[0] == ';') {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key =
            Trim(line.substr(0, equals));
        const std::string value =
            Trim(line.substr(equals + 1));

        if (key == "game_path") {
            config.game_path = value;
        } else if (key == "client_dll") {
            config.client_dll = value;
        } else if (key == "target_process") {
            config.target_process = Widen(value);
        } else if (key == "wait_for_game_ms") {
            config.wait_for_game_ms =
                ParseU32(value, config.wait_for_game_ms);
        } else if (key == "require_x64") {
            config.require_x64 =
                ParseBool(value, config.require_x64);
        } else if (key == "expected_timestamp") {
            config.expected_timestamp =
                ParseU32(value, config.expected_timestamp);
        } else if (key == "expected_image_size") {
            config.expected_image_size =
                ParseU32(value, config.expected_image_size);
        } else if (key == "expected_file_size") {
            config.expected_file_size =
                ParseU64(value, config.expected_file_size);
        } else if (key == "expected_machine") {
            config.expected_machine =
                ParseU16(value, config.expected_machine);
        } else if (key == "expected_text_rva") {
            config.expected_text_rva =
                ParseU32(value, config.expected_text_rva);
        } else if (key == "expected_text_size") {
            config.expected_text_size =
                ParseU32(value, config.expected_text_size);
        } else if (key == "expected_text_fnv1a64") {
            config.expected_text_fnv1a64 =
                ParseU64(value, config.expected_text_fnv1a64);
        } else if (key == "expected_file_version") {
            config.expected_file_version = value;
        } else if (key == "expected_sha256") {
            const auto normalized =
                NormalizeSha256(value);

            config.expected_sha256 =
                IsSha256(normalized)
                    ? normalized
                    : std::string{};
        }
    }

    return config;
}

}

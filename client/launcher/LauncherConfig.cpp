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
        std::find_if(
            value.begin(),
            value.end(),
            not_space));

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            not_space).base(),
        value.end());

    return value;
}

std::wstring Widen(
    const std::string& value) {

    if (value.empty()) {
        return {};
    }

    const int length =
        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

    if (length <= 0) {
        return {};
    }

    std::wstring result(
        static_cast<std::size_t>(length),
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length);

    return result;
}

bool ParseBool(
    const std::string& value,
    bool fallback) {

    std::string normalized =
        value;

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
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

}

LauncherConfig LauncherConfigReader::Load(
    const std::filesystem::path& path) const {

    LauncherConfig config;

    std::ifstream input(path);
    if (!input) {
        return config;
    }

    std::string line;

    while (std::getline(
        input,
        line)) {

        line = Trim(line);

        if (line.empty() ||
            line[0] == '#' ||
            line[0] == ';') {
            continue;
        }

        const auto equals =
            line.find('=');

        if (equals == std::string::npos) {
            continue;
        }

        const std::string key =
            Trim(line.substr(
                0,
                equals));

        const std::string value =
            Trim(line.substr(
                equals + 1));

        if (key == "game_path") {
            config.game_path = value;
        } else if (
            key == "client_dll") {

            config.client_dll = value;

        } else if (
            key == "target_process") {

            config.target_process =
                Widen(value);

        } else if (
            key == "wait_for_game_ms") {

            try {
                config.wait_for_game_ms =
                    static_cast<
                        std::uint32_t>(
                        std::stoul(value));
            } catch (...) {
            }

        } else if (
            key == "require_x64") {

            config.require_x64 =
                ParseBool(
                    value,
                    config.require_x64);
        }
    }

    return config;
}

}

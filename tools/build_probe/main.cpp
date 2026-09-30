#include "../../client/launcher/GameBuild.hpp"

#include <Windows.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

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
        length >= sizeof(path) / sizeof(path[0])) {
        return std::filesystem::current_path();
    }

    return std::filesystem::path(path).parent_path();
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

}

int wmain(int argc, wchar_t** argv) {
    const auto root =
        ExecutableDirectory();

    const std::filesystem::path target =
        argc >= 2
            ? std::filesystem::path(argv[1])
            : root / "RDR.exe";

    const auto build =
        dustwire::launcher::GameBuild().Inspect(
            target.wstring());

    if (!build.valid_pe) {
        std::wcerr
            << L"Fingerprint failed: "
            << build.description
            << L"\n";
        return 2;
    }

    std::cout
        << "path=" << Narrow(build.path)
        << "\n"
        << "machine=0x"
        << std::hex << build.machine
        << "\n"
        << "x64="
        << std::dec << (build.is_64_bit ? 1 : 0)
        << "\n"
        << "timestamp=0x"
        << std::hex << build.timestamp
        << "\n"
        << "image_size=0x"
        << build.image_size
        << "\n"
        << "file_size="
        << std::dec << build.file_size
        << "\n"
        << "text_rva=0x"
        << std::hex << build.text_rva
        << "\n"
        << "text_size=0x"
        << build.text_size
        << "\n"
        << "text_fnv1a64=0x"
        << build.text_fnv1a64
        << "\n"
        << "file_version="
        << (build.file_version.empty()
            ? "<unknown>"
            : build.file_version)
        << "\n"
        << "sha256="
        << build.sha256
        << "\n";

    return 0;
}

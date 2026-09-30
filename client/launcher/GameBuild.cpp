#include "GameBuild.hpp"

#include <windows.h>

#include <fstream>

namespace dustwire::launcher {

GameBuildInfo GameBuild::Inspect(
    const std::wstring& executable_path) const {

    GameBuildInfo result;
    result.path = executable_path;

    HANDLE file = CreateFileW(
        executable_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ |
        FILE_SHARE_WRITE |
        FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        result.description =
            L"could not open executable";
        return result;
    }

    LARGE_INTEGER file_size{};
    if (GetFileSizeEx(file, &file_size)) {
        result.file_size =
            static_cast<std::uint64_t>(
                file_size.QuadPart);
    }

    IMAGE_DOS_HEADER dos{};
    DWORD read = 0;

    if (!ReadFile(
            file,
            &dos,
            sizeof(dos),
            &read,
            nullptr) ||
        read != sizeof(dos) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0) {

        CloseHandle(file);
        result.description =
            L"invalid DOS/PE header";
        return result;
    }

    if (SetFilePointer(
            file,
            dos.e_lfanew,
            nullptr,
            FILE_BEGIN) ==
        INVALID_SET_FILE_POINTER &&
        GetLastError() != NO_ERROR) {

        CloseHandle(file);
        result.description =
            L"could not seek to PE header";
        return result;
    }

    DWORD signature = 0;

    if (!ReadFile(
            file,
            &signature,
            sizeof(signature),
            &read,
            nullptr) ||
        read != sizeof(signature) ||
        signature != IMAGE_NT_SIGNATURE) {

        CloseHandle(file);
        result.description =
            L"invalid PE signature";
        return result;
    }

    IMAGE_FILE_HEADER file_header{};

    if (!ReadFile(
            file,
            &file_header,
            sizeof(file_header),
            &read,
            nullptr) ||
        read != sizeof(file_header)) {

        CloseHandle(file);
        result.description =
            L"could not read PE file header";
        return result;
    }

    result.machine =
        file_header.Machine;
    result.timestamp =
        file_header.TimeDateStamp;

    WORD optional_magic = 0;

    if (!ReadFile(
            file,
            &optional_magic,
            sizeof(optional_magic),
            &read,
            nullptr) ||
        read != sizeof(optional_magic)) {

        CloseHandle(file);
        result.description =
            L"could not read optional header";
        return result;
    }

    if (optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 optional{};
        optional.Magic = optional_magic;

        if (!ReadFile(
                file,
                reinterpret_cast<BYTE*>(&optional) +
                    sizeof(optional_magic),
                sizeof(optional) -
                    sizeof(optional_magic),
                &read,
                nullptr) ||
            read != sizeof(optional) -
                        sizeof(optional_magic)) {

            CloseHandle(file);
            result.description =
                L"could not read PE64 optional header";
            return result;
        }

        result.is_64_bit = true;
        result.image_size =
            optional.SizeOfImage;
        result.valid_pe = true;
    } else if (
        optional_magic ==
        IMAGE_NT_OPTIONAL_HDR32_MAGIC) {

        IMAGE_OPTIONAL_HEADER32 optional{};
        optional.Magic = optional_magic;

        if (!ReadFile(
                file,
                reinterpret_cast<BYTE*>(&optional) +
                    sizeof(optional_magic),
                sizeof(optional) -
                    sizeof(optional_magic),
                &read,
                nullptr) ||
            read != sizeof(optional) -
                        sizeof(optional_magic)) {

            CloseHandle(file);
            result.description =
                L"could not read PE32 optional header";
            return result;
        }

        result.is_64_bit = false;
        result.image_size =
            optional.SizeOfImage;
        result.valid_pe = true;
    } else {
        result.description =
            L"unknown PE optional-header type";
    }

    CloseHandle(file);

    if (result.valid_pe) {
        result.description =
            result.is_64_bit
                ? L"valid PE64 executable"
                : L"valid PE32 executable";
    }

    return result;
}

bool GameBuild::IsSupported(
    const GameBuildInfo& build,
    std::wstring& reason) const {

    if (!build.valid_pe) {
        reason = L"RDR.exe is not a valid PE executable.";
        return false;
    }

    if (!build.is_64_bit) {
        reason =
            L"RDR.exe is PE32, but DustwireMP v0.3 "
            L"requires the x64 client build.";
        return false;
    }

    if (build.machine != IMAGE_FILE_MACHINE_AMD64) {
        reason =
            L"RDR.exe does not report IMAGE_FILE_MACHINE_AMD64.";
        return false;
    }

    reason = L"x64 PE build accepted for v0.3 bootstrap.";
    return true;
}

}

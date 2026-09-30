#include "GameBuild.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace dustwire::launcher {

namespace {

bool Sha256File(HANDLE file, std::string& output) {
    LARGE_INTEGER origin{};
    if (!SetFilePointerEx(file, origin, nullptr, FILE_BEGIN)) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_length = 0;
    DWORD result_length = 0;
    bool success = false;

    do {
        if (BCryptOpenAlgorithmProvider(
                &algorithm,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                0) != 0) {
            break;
        }

        if (BCryptGetProperty(
                algorithm,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&object_length),
                sizeof(object_length),
                &result_length,
                0) != 0 ||
            object_length == 0) {
            break;
        }

        DWORD hash_length = 0;
        if (BCryptGetProperty(
                algorithm,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hash_length),
                sizeof(hash_length),
                &result_length,
                0) != 0 ||
            hash_length != 32) {
            break;
        }

        std::vector<UCHAR> hash_object(object_length);
        std::array<UCHAR, 32> digest{};

        if (BCryptCreateHash(
                algorithm,
                &hash,
                hash_object.data(),
                object_length,
                nullptr,
                0,
                0) != 0) {
            break;
        }

        std::array<UCHAR, 64 * 1024> buffer{};
        for (;;) {
            DWORD bytes_read = 0;

            if (!ReadFile(
                    file,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()),
                    &bytes_read,
                    nullptr)) {
                success = false;
                break;
            }

            if (bytes_read == 0) {
                success = true;
                break;
            }

            if (BCryptHashData(
                    hash,
                    buffer.data(),
                    bytes_read,
                    0) != 0) {
                success = false;
                break;
            }
        }

        if (!success) {
            break;
        }

        if (BCryptFinishHash(
                hash,
                digest.data(),
                static_cast<ULONG>(digest.size()),
                0) != 0) {
            success = false;
            break;
        }

        std::ostringstream hex;
        hex << std::hex << std::setfill('0');

        for (const UCHAR byte : digest) {
            hex << std::setw(2)
                << static_cast<unsigned int>(byte);
        }

        output = hex.str();
    } while (false);

    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }

    if (algorithm != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }

    origin.QuadPart = 0;
    SetFilePointerEx(file, origin, nullptr, FILE_BEGIN);
    return success;
}

}

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
        result.description = L"could not open executable";
        return result;
    }

    LARGE_INTEGER file_size{};
    if (GetFileSizeEx(file, &file_size) &&
        file_size.QuadPart >= 0) {
        result.file_size =
            static_cast<std::uint64_t>(file_size.QuadPart);
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
        result.description = L"invalid DOS/PE header";
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
        result.description = L"invalid PE signature";
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

    result.machine = file_header.Machine;
    result.timestamp = file_header.TimeDateStamp;

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
        result.image_size = optional.SizeOfImage;
        result.valid_pe = true;
    } else if (
        optional_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {

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
        result.image_size = optional.SizeOfImage;
        result.valid_pe = true;
    } else {
        result.description =
            L"unknown PE optional-header type";
    }

    if (result.valid_pe) {
        if (!Sha256File(file, result.sha256)) {
            result.description =
                result.is_64_bit
                    ? L"valid PE64 executable; SHA-256 unavailable"
                    : L"valid PE32 executable; SHA-256 unavailable";
        } else {
            result.description =
                result.is_64_bit
                    ? L"valid PE64 executable"
                    : L"valid PE32 executable";
        }
    }

    CloseHandle(file);
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

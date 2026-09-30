#include "GameBuild.hpp"

#include <windows.h>
#include <bcrypt.h>
#include <winver.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "version.lib")

namespace dustwire::launcher {

namespace {

std::uint64_t Fnv1a64(
    const std::uint8_t* data,
    std::size_t size) {

    constexpr std::uint64_t offset =
        14695981039346656037ULL;
    constexpr std::uint64_t prime =
        1099511628211ULL;

    std::uint64_t hash = offset;

    for (std::size_t i = 0; i < size; ++i) {
        hash =
            (hash ^ data[i]) *
            prime;
    }

    return hash;
}

bool Sha256File(
    HANDLE file,
    std::string& output) {

    LARGE_INTEGER origin{};
    if (!SetFilePointerEx(
            file,
            origin,
            nullptr,
            FILE_BEGIN)) {
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
                reinterpret_cast<PUCHAR>(
                    &object_length),
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
                reinterpret_cast<PUCHAR>(
                    &hash_length),
                sizeof(hash_length),
                &result_length,
                0) != 0 ||
            hash_length != 32) {
            break;
        }

        std::vector<UCHAR> hash_object(
            object_length);
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
                    static_cast<DWORD>(
                        buffer.size()),
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
                static_cast<ULONG>(
                    digest.size()),
                0) != 0) {
            success = false;
            break;
        }

        std::ostringstream hex;
        hex << std::hex << std::setfill('0');

        for (const UCHAR byte : digest) {
            hex << std::setw(2)
                << static_cast<unsigned int>(
                    byte);
        }

        output = hex.str();
    } while (false);

    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }

    if (algorithm != nullptr) {
        BCryptCloseAlgorithmProvider(
            algorithm,
            0);
    }

    origin.QuadPart = 0;
    SetFilePointerEx(
        file,
        origin,
        nullptr,
        FILE_BEGIN);

    return success;
}

std::string FileVersion(
    const std::wstring& path) {

    const DWORD handle = 0;
    const DWORD size =
        GetFileVersionInfoSizeW(
            path.c_str(),
            const_cast<DWORD*>(&handle));

    if (size == 0) {
        return {};
    }

    std::vector<std::uint8_t> buffer(size);

    if (!GetFileVersionInfoW(
            path.c_str(),
            0,
            size,
            buffer.data())) {
        return {};
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;

    if (!VerQueryValueW(
            buffer.data(),
            L"\\",
            reinterpret_cast<void**>(
                &info),
            &length) ||
        info == nullptr ||
        length < sizeof(VS_FIXEDFILEINFO)) {
        return {};
    }

    std::ostringstream version;
    version
        << HIWORD(info->dwFileVersionMS)
        << '.'
        << LOWORD(info->dwFileVersionMS)
        << '.'
        << HIWORD(info->dwFileVersionLS)
        << '.'
        << LOWORD(info->dwFileVersionLS);

    return version.str();
}

bool ReadExact(
    HANDLE file,
    void* buffer,
    DWORD size) {

    DWORD read = 0;

    return ReadFile(
               file,
               buffer,
               size,
               &read,
               nullptr) &&
           read == size;
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
        result.description =
            L"could not open executable";
        return result;
    }

    LARGE_INTEGER file_size{};
    if (GetFileSizeEx(
            file,
            &file_size) &&
        file_size.QuadPart >= 0) {
        result.file_size =
            static_cast<std::uint64_t>(
                file_size.QuadPart);
    }

    IMAGE_DOS_HEADER dos{};

    if (!ReadExact(
            file,
            &dos,
            sizeof(dos)) ||
        dos.e_magic !=
            IMAGE_DOS_SIGNATURE ||
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

    if (!ReadExact(
            file,
            &signature,
            sizeof(signature)) ||
        signature != IMAGE_NT_SIGNATURE) {

        CloseHandle(file);
        result.description =
            L"invalid PE signature";
        return result;
    }

    IMAGE_FILE_HEADER file_header{};

    if (!ReadExact(
            file,
            &file_header,
            sizeof(file_header))) {

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

    if (!ReadExact(
            file,
            &optional_magic,
            sizeof(optional_magic))) {

        CloseHandle(file);
        result.description =
            L"could not read optional header";
        return result;
    }

    if (optional_magic ==
        IMAGE_NT_OPTIONAL_HDR64_MAGIC) {

        IMAGE_OPTIONAL_HEADER64 optional{};
        optional.Magic = optional_magic;

        if (!ReadExact(
                file,
                reinterpret_cast<BYTE*>(
                    &optional) +
                    sizeof(optional_magic),
                static_cast<DWORD>(
                    sizeof(optional) -
                    sizeof(optional_magic)))) {

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

        if (!ReadExact(
                file,
                reinterpret_cast<BYTE*>(
                    &optional) +
                    sizeof(optional_magic),
                static_cast<DWORD>(
                    sizeof(optional) -
                    sizeof(optional_magic)))) {

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

    if (result.valid_pe) {
        IMAGE_SECTION_HEADER section{};
        const DWORD section_count =
            file_header.NumberOfSections;

        for (DWORD i = 0; i < section_count; ++i) {
            if (!ReadExact(
                    file,
                    &section,
                    sizeof(section))) {
                result.valid_pe = false;
                result.description =
                    L"could not read PE sections";
                break;
            }

            char name[9]{};
            std::memcpy(
                name,
                section.Name,
                8);

            if (std::string(name) !=
                ".text") {
                continue;
            }

            break;
        }
    }

    if (result.valid_pe) {
        // Re-read the section table from disk and hash the raw .text bytes.
        LARGE_INTEGER section_table{};
        section_table.QuadPart =
            static_cast<LONGLONG>(
                dos.e_lfanew) +
            sizeof(DWORD) +
            sizeof(IMAGE_FILE_HEADER) +
            file_header.SizeOfOptionalHeader;

        if (!SetFilePointerEx(
                file,
                section_table,
                nullptr,
                FILE_BEGIN)) {
            result.valid_pe = false;
            result.description =
                L"could not seek to section table";
        } else {
            bool found_text = false;

            for (DWORD i = 0;
                 i < file_header.NumberOfSections;
                 ++i) {

                IMAGE_SECTION_HEADER section{};

                if (!ReadExact(
                        file,
                        &section,
                        sizeof(section))) {
                    result.valid_pe = false;
                    result.description =
                        L"could not read PE sections";
                    break;
                }

                char name[9]{};
                std::memcpy(
                    name,
                    section.Name,
                    8);

                if (std::string(name) !=
                    ".text") {
                    continue;
                }

                result.text_rva =
                    section.VirtualAddress;
                result.text_size =
                    section.Misc.VirtualSize;

                const auto raw_offset =
                    static_cast<std::uint64_t>(
                        section.PointerToRawData);
                const auto raw_size =
                    static_cast<std::size_t>(
                        section.SizeOfRawData);
                const auto hash_size =
                    std::min<std::size_t>(
                        section.Misc.VirtualSize,
                        raw_size);

                if (raw_offset + hash_size >
                    result.file_size) {
                    result.valid_pe = false;
                    result.description =
                        L".text section exceeds file bounds";
                    break;
                }

                LARGE_INTEGER text_position{};
                text_position.QuadPart =
                    static_cast<LONGLONG>(
                        raw_offset);

                if (!SetFilePointerEx(
                        file,
                        text_position,
                        nullptr,
                        FILE_BEGIN)) {
                    result.valid_pe = false;
                    result.description =
                        L"could not seek to .text";
                    break;
                }

                std::vector<std::uint8_t> text(
                    hash_size);

                if (!text.empty() &&
                    !ReadExact(
                        file,
                        text.data(),
                        static_cast<DWORD>(
                            text.size()))) {
                    result.valid_pe = false;
                    result.description =
                        L"could not read .text";
                    break;
                }

                result.text_fnv1a64 =
                    Fnv1a64(
                        text.data(),
                        text.size());
                found_text = true;
                break;
            }

            if (result.valid_pe &&
                !found_text) {
                result.valid_pe = false;
                result.description =
                    L"PE .text section not found";
            }
        }
    }

    if (result.valid_pe) {
        result.file_version =
            FileVersion(executable_path);

        if (!Sha256File(
                file,
                result.sha256)) {
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
        reason =
            L"RDR.exe is not a valid PE executable.";
        return false;
    }

    if (!build.is_64_bit) {
        reason =
            L"RDR.exe is PE32, but DustwireMP v0.3 "
            L"requires the x64 client build.";
        return false;
    }

    if (build.machine !=
        IMAGE_FILE_MACHINE_AMD64) {
        reason =
            L"RDR.exe does not report IMAGE_FILE_MACHINE_AMD64.";
        return false;
    }

    reason =
        L"x64 PE build accepted for v0.3 bootstrap.";
    return true;
}

}

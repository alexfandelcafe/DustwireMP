#include "NativeInvoker.hpp"

#include "PatternScanner.hpp"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace dustwire::rdr1 {

namespace {

constexpr const char* kNativeRegistrationPattern =
    "4C 8B 1D ? ? ? ? 41 8B C1";

constexpr std::uint32_t kRegistrationSentinel = 0xA0AE0C98u;
constexpr std::uint32_t kGetPosition = 0x99BD9D6Fu;
constexpr std::uint32_t kGetGameState = 0xDD9BD22Bu;

constexpr std::uint32_t kNativeScrThreadWait = 0x7715C03Bu;
constexpr std::uint32_t kNativeGetThisScriptId = 0x9C424E0Du;
constexpr std::uint32_t kNativeGetScriptName = 0x0BC52445u;

#ifdef _WIN32
bool GuardedCopy(
    const void* source,
    void* destination,
    std::size_t size) noexcept {

    if (!source || !destination || size == 0) {
        return false;
    }

    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#endif

}

std::uintptr_t NativeInvoker::ResolveRipTarget(
    std::uintptr_t instruction) const {

    if (!Readable(
            instruction + 3,
            sizeof(std::int32_t))) {
        return 0;
    }

    std::int32_t displacement = 0;

    if (!GuardedCopy(
            reinterpret_cast<const void*>(instruction + 3),
            &displacement,
            sizeof(displacement))) {
        return 0;
    }

    return instruction +
           7 +
           static_cast<std::intptr_t>(displacement);
}

bool NativeInvoker::Readable(
    std::uintptr_t address,
    std::size_t size) const {

    if (!address || !size) {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi{};

    if (VirtualQuery(
            reinterpret_cast<const void*>(address),
            &mbi,
            sizeof(mbi)) != sizeof(mbi)) {
        return false;
    }

    if (mbi.State != MEM_COMMIT ||
        (mbi.Protect & PAGE_GUARD) != 0 ||
        (mbi.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }

    const auto begin =
        reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
    const auto end =
        begin + mbi.RegionSize;

    if (address < begin ||
        address >= end ||
        size > end - address) {
        return false;
    }

    constexpr DWORD readable_flags =
        PAGE_READONLY |
        PAGE_READWRITE |
        PAGE_WRITECOPY |
        PAGE_EXECUTE_READ |
        PAGE_EXECUTE_READWRITE |
        PAGE_EXECUTE_WRITECOPY;

    return (mbi.Protect & readable_flags) != 0;
}

std::uintptr_t NativeInvoker::FindEntryInTable(
    std::uintptr_t table,
    std::uint32_t modulator,
    std::uint32_t hash) const {

    if (!table ||
        modulator == 0 ||
        modulator > 0x100000u) {
        return 0;
    }

    std::uint32_t temp_hash = hash;
    std::uint32_t index = hash % modulator;

    for (std::uint32_t probes = 0;
         probes < modulator;
         ++probes) {

        const auto entry =
            table +
            static_cast<std::uintptr_t>(index) * 16u;

        if (!Readable(entry, 16)) {
            return 0;
        }

        std::uint32_t entry_hash = 0;

        if (!GuardedCopy(
                reinterpret_cast<const void*>(entry),
                &entry_hash,
                sizeof(entry_hash))) {
            return 0;
        }

        if (entry_hash == hash) {
            return entry;
        }

        if (entry_hash == 0) {
            return 0;
        }

        temp_hash =
            (temp_hash >> 1u) + 1u;
        index =
            (temp_hash + index) % modulator;
    }

    return 0;
}

std::uintptr_t NativeInvoker::FindHandlerInTable(
    std::uintptr_t table,
    std::uint32_t modulator,
    std::uint32_t hash) const {

    const auto entry =
        FindEntryInTable(table, modulator, hash);

    if (!entry) {
        return 0;
    }

    std::uintptr_t handler = 0;

    if (!GuardedCopy(
            reinterpret_cast<const void*>(entry + 8),
            &handler,
            sizeof(handler))) {
        return 0;
    }

    return handler;
}

bool NativeInvoker::ReadTable(
    std::uintptr_t storage,
    std::uintptr_t& table,
    std::uint32_t& modulator,
    std::string* diagnostics) const {

    table = 0;
    modulator = 0;

    if (diagnostics) {
        diagnostics->clear();
    }

    if (!storage ||
        !Readable(
            storage,
            sizeof(std::uintptr_t))) {
        if (diagnostics) {
            *diagnostics = "native registration storage unreadable";
        }
        return false;
    }

    std::uintptr_t raw_table = 0;
    std::uint32_t raw_modulator = 0;

    if (!GuardedCopy(
            reinterpret_cast<const void*>(storage),
            &raw_table,
            sizeof(raw_table))) {
        if (diagnostics) {
            *diagnostics = "native registration table pointer read failed";
        }
        return false;
    }

    // The global points at the registration table object. Its modulator is
    // stored at table + 0x8, not at the global storage address + 0x8.
    if (!raw_table ||
        !Readable(
            raw_table + sizeof(std::uintptr_t),
            sizeof(raw_modulator)) ||
        !GuardedCopy(
            reinterpret_cast<const void*>(
                raw_table + sizeof(std::uintptr_t)),
            &raw_modulator,
            sizeof(raw_modulator))) {
        if (diagnostics) {
            *diagnostics = "native registration table modulator read failed";
        }
        return false;
    }

    if (raw_modulator == 0 ||
        raw_modulator > 0x100000u ||
        !Readable(raw_table, 16)) {
        if (diagnostics) {
            char buffer[192]{};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "native table invalid table=0x%llX mod=%u",
                static_cast<unsigned long long>(raw_table),
                raw_modulator);
            *diagnostics = buffer;
        }
        return false;
    }

    table = raw_table;
    modulator = raw_modulator;

    if (diagnostics) {
        char buffer[192]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "table=0x%llX mod=%u",
            static_cast<unsigned long long>(table),
            modulator);
        *diagnostics = buffer;
    }

    return true;
}

bool NativeInvoker::Initialize(
    std::uintptr_t module_base,
    std::size_t image_size,
    std::uintptr_t text_rva,
    std::size_t text_size) {

    if (ready_) {
        return true;
    }

    if (!module_base ||
        !image_size ||
        !text_rva ||
        !text_size) {
        last_error_ = "invalid RDR1 module metadata";
        return false;
    }

    if (module_base_ != module_base ||
        image_size_ != image_size) {
        module_base_ = module_base;
        image_size_ = image_size;
        native_registration_storage_ = 0;
        registration_storage_resolved_ = false;
        ready_ = false;
        last_error_.clear();
    }

    if (!registration_storage_resolved_) {
        const auto pattern =
            BytePattern::Parse(
                kNativeRegistrationPattern);

        if (!pattern ||
            pattern->bytes.empty() ||
            text_size < pattern->bytes.size()) {
            last_error_ =
                "invalid RDR1 native registration pattern";
            return false;
        }

        const auto* text =
            reinterpret_cast<const std::uint8_t*>(
                module_base_ + text_rva);

        for (std::size_t offset = 0;
             offset + pattern->bytes.size() <= text_size;
             ++offset) {

            bool match = true;

            for (std::size_t i = 0;
                 i < pattern->bytes.size();
                 ++i) {
                if (pattern->bytes[i].has_value() &&
                    text[offset + i] !=
                        pattern->bytes[i].value()) {
                    match = false;
                    break;
                }
            }

            if (!match) {
                continue;
            }

            const auto hit =
                reinterpret_cast<std::uintptr_t>(
                    text + offset);

            const auto storage =
                ResolveRipTarget(hit);

            if (!PatternScanner::ValidateInModule(
                    storage,
                    module_base_,
                    image_size_)) {
                continue;
            }

            std::uintptr_t table = 0;
            std::uint32_t modulator = 0;

            if (!ReadTable(
                    storage,
                    table,
                    modulator)) {
                continue;
            }

            // The registration table itself is the lifecycle-independent
            // prerequisite. Gameplay natives such as GET_GAME_STATE may not be
            // populated yet during the intro/menu bootstrap, so do not gate
            // registration-storage resolution on them.
            native_registration_storage_ =
                storage;

            const auto sentinel =
                FindHandlerInTable(
                    table,
                    modulator,
                    kRegistrationSentinel);

            const auto get_position =
                FindHandlerInTable(
                    table,
                    modulator,
                    kGetPosition);

            const auto get_game_state =
                FindHandlerInTable(
                    table,
                    modulator,
                    kGetGameState);

            char diagnostics[320]{};
            std::snprintf(
                diagnostics,
                sizeof(diagnostics),
                "[DustwireRDR1] native table candidate storage=0x%llX "
                "table=0x%llX mod=%u sentinel=%s getPosition=%s "
                "getGameState=%s",
                static_cast<unsigned long long>(storage),
                static_cast<unsigned long long>(table),
                modulator,
                sentinel ? "yes" : "no",
                get_position ? "yes" : "no",
                get_game_state ? "yes" : "no");
            OutputDebugStringA(diagnostics);
            OutputDebugStringA("\n");

            registration_storage_resolved_ =
                true;
            last_error_.clear();
            break;
            registration_storage_resolved_ =
                true;

            char buffer[256]{};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "[DustwireRDR1] native table ready storage=0x%llX "
                "table=0x%llX mod=%u",
                static_cast<unsigned long long>(
                    storage),
                static_cast<unsigned long long>(
                    table),
                modulator);
            OutputDebugStringA(buffer);
            OutputDebugStringA("\n");
            break;
        }

        if (!registration_storage_resolved_) {
            last_error_ =
                "RDR1 native registration table not initialized yet";
            return false;
        }
    }

    std::uintptr_t table = 0;
    std::uint32_t modulator = 0;
    std::string diagnostics;

    if (!ReadTable(
            native_registration_storage_,
            table,
            modulator,
            &diagnostics)) {
        last_error_ =
            "RDR1 native table unavailable: " +
            diagnostics;
        return false;
    }

    const auto wait_handler =
        FindHandlerInTable(
            table,
            modulator,
            kNativeScrThreadWait);

    const auto script_id_handler =
        FindHandlerInTable(
            table,
            modulator,
            kNativeGetThisScriptId);

    const auto script_name_handler =
        FindHandlerInTable(
            table,
            modulator,
            kNativeGetScriptName);

    if (!wait_handler ||
        !script_id_handler ||
        !script_name_handler) {
        last_error_ =
            "RDR1 native table missing game-thread handlers";
        return false;
    }

    ready_ = true;
    last_error_.clear();
    return true;
}

NativeInvoker::NativeHandler NativeInvoker::GetHandler(
    std::uint32_t hash) const {

    if (!ready_ ||
        !native_registration_storage_) {
        return nullptr;
    }

    std::uintptr_t table = 0;
    std::uint32_t modulator = 0;

    if (!ReadTable(
            native_registration_storage_,
            table,
            modulator)) {
        return nullptr;
    }

    const auto handler =
        FindHandlerInTable(
            table,
            modulator,
            hash);

    return reinterpret_cast<NativeHandler>(handler);
}

bool NativeInvoker::CurrentHandler(
    std::uint32_t hash,
    NativeHandler& out) const {

    out = GetHandler(hash);
    return out != nullptr;
}

bool NativeInvoker::HookNative(
    std::uint32_t hash,
    NativeHandler replacement,
    NativeHandler& original,
    std::string* error) {

    original = nullptr;

    if (error) {
        error->clear();
    }

    if (!ready_ ||
        !native_registration_storage_) {
        if (error) {
            *error = "RDR1 native invoker not ready";
        }
        return false;
    }

    if (!replacement) {
        if (error) {
            *error = "RDR1 native replacement is null";
        }
        return false;
    }

    std::uintptr_t table = 0;
    std::uint32_t modulator = 0;

    if (!ReadTable(
            native_registration_storage_,
            table,
            modulator,
            error)) {
        return false;
    }

    const auto entry =
        FindEntryInTable(
            table,
            modulator,
            hash);

    if (!entry) {
        if (error) {
            char buffer[96]{};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "RDR1 native handler not found: 0x%08X",
                hash);
            *error = buffer;
        }
        return false;
    }

    std::uintptr_t current = 0;

    if (!GuardedCopy(
            reinterpret_cast<const void*>(entry + 8),
            &current,
            sizeof(current)) ||
        !current) {
        if (error) {
            *error = "RDR1 native handler pointer unavailable";
        }
        return false;
    }

    DWORD old_protect = 0;

    if (!VirtualProtect(
            reinterpret_cast<void*>(entry + 8),
            sizeof(std::uintptr_t),
            PAGE_READWRITE,
            &old_protect)) {
        if (error) {
            *error = "RDR1 native table protection change failed";
        }
        return false;
    }

    const auto previous =
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(entry + 8),
            reinterpret_cast<PVOID>(
                reinterpret_cast<std::uintptr_t>(
                    replacement)));

    DWORD ignored = 0;

    VirtualProtect(
        reinterpret_cast<void*>(entry + 8),
        sizeof(std::uintptr_t),
        old_protect,
        &ignored);

    original =
        reinterpret_cast<NativeHandler>(previous);

    return original != nullptr;
}

bool NativeInvoker::UnhookNative(
    std::uint32_t hash,
    NativeHandler replacement,
    NativeHandler original) {

    if (!ready_ ||
        !native_registration_storage_ ||
        !replacement ||
        !original) {
        return false;
    }

    std::uintptr_t table = 0;
    std::uint32_t modulator = 0;

    if (!ReadTable(
            native_registration_storage_,
            table,
            modulator)) {
        return false;
    }

    const auto entry =
        FindEntryInTable(
            table,
            modulator,
            hash);

    if (!entry) {
        return false;
    }

    std::uintptr_t current = 0;

    if (!GuardedCopy(
            reinterpret_cast<const void*>(entry + 8),
            &current,
            sizeof(current))) {
        return false;
    }

    if (current !=
        reinterpret_cast<std::uintptr_t>(
            replacement)) {
        return false;
    }

    DWORD old_protect = 0;

    if (!VirtualProtect(
            reinterpret_cast<void*>(entry + 8),
            sizeof(std::uintptr_t),
            PAGE_READWRITE,
            &old_protect)) {
        return false;
    }

    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(entry + 8),
        reinterpret_cast<PVOID>(
            reinterpret_cast<std::uintptr_t>(
                original)));

    DWORD ignored = 0;

    VirtualProtect(
        reinterpret_cast<void*>(entry + 8),
        sizeof(std::uintptr_t),
        old_protect,
        &ignored);

    return true;
}

}

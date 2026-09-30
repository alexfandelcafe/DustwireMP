#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dustwire::rdr1 {

class NativeInvoker final {
public:
    using NativeHandler = void(*)(void* context);

    bool Initialize(
        std::uintptr_t module_base,
        std::size_t image_size,
        std::uintptr_t text_rva,
        std::size_t text_size);

    bool Ready() const { return ready_; }
    const std::string& LastError() const { return last_error_; }

    bool CurrentHandler(
        std::uint32_t hash,
        NativeHandler& out) const;

    bool HookNative(
        std::uint32_t hash,
        NativeHandler replacement,
        NativeHandler& original,
        std::string* error = nullptr);

    bool UnhookNative(
        std::uint32_t hash,
        NativeHandler replacement,
        NativeHandler original);

private:
    NativeHandler GetHandler(
        std::uint32_t hash) const;

    std::uintptr_t FindHandlerInTable(
        std::uintptr_t table,
        std::uint32_t modulator,
        std::uint32_t hash) const;

    std::uintptr_t FindEntryInTable(
        std::uintptr_t table,
        std::uint32_t modulator,
        std::uint32_t hash) const;

    bool ReadTable(
        std::uintptr_t storage,
        std::uintptr_t& table,
        std::uint32_t& modulator,
        std::string* diagnostics = nullptr) const;

    std::uintptr_t ResolveRipTarget(
        std::uintptr_t instruction) const;

    bool Readable(
        std::uintptr_t address,
        std::size_t size) const;

    std::uintptr_t module_base_{};
    std::size_t image_size_{};
    std::uintptr_t native_registration_storage_{};
    bool registration_storage_resolved_{};
    bool ready_{};
    std::string last_error_;
};

}

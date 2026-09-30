#include "Rdr1GameTick.hpp"

#include <Windows.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace dustwire::client {

namespace {

struct MainModuleLayout final {
    std::uintptr_t base{};
    std::size_t image_size{};
    std::uint32_t text_rva{};
    std::size_t text_size{};
};

bool ReadMainModuleLayout(
    MainModuleLayout& result) {

    const auto module =
        GetModuleHandleW(nullptr);

    if (!module) {
        return false;
    }

    const auto base =
        reinterpret_cast<std::uintptr_t>(module);

    const auto* dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(base);

    if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0) {
        return false;
    }

    const auto* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            base +
            static_cast<std::uintptr_t>(
                dos->e_lfanew));

    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine !=
            IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic !=
            IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return false;
    }

    result.base = base;
    result.image_size =
        nt->OptionalHeader.SizeOfImage;

    const auto* section =
        IMAGE_FIRST_SECTION(
            const_cast<IMAGE_NT_HEADERS64*>(nt));

    for (WORD i = 0;
         i < nt->FileHeader.NumberOfSections;
         ++i) {

        char name[9]{};
        std::memcpy(
            name,
            section[i].Name,
            8);

        if (std::string(name) != ".text") {
            continue;
        }

        result.text_rva =
            section[i].VirtualAddress;

        result.text_size =
            section[i].Misc.VirtualSize;

        break;
    }

    return result.base != 0 &&
           result.image_size != 0 &&
           result.text_rva != 0 &&
           result.text_size != 0;
}

}

bool Rdr1GameTickSource::Start(
    std::function<void()> callback) {

    if (running_.exchange(
            true,
            std::memory_order_acq_rel)) {
        return false;
    }

    if (!callback) {
        running_.store(
            false,
            std::memory_order_release);
        return false;
    }

    callback_ = std::move(callback);
    ready_.store(false, std::memory_order_release);

    worker_ =
        std::thread(
            &Rdr1GameTickSource::Bootstrap,
            this);

    return true;
}

void Rdr1GameTickSource::Stop() {
    if (!running_.exchange(
            false,
            std::memory_order_acq_rel)) {
        return;
    }

    dispatcher_.Detach();

    if (worker_.joinable()) {
        worker_.join();
    }

    ready_.store(false, std::memory_order_release);
    recurring_task_.reset();
    callback_ = {};
}

bool Rdr1GameTickSource::InitializeRdr1() {
    MainModuleLayout layout{};

    if (!ReadMainModuleLayout(layout)) {
        return false;
    }

    if (!native_invoker_.Initialize(
            layout.base,
            layout.image_size,
            layout.text_rva,
            layout.text_size)) {
        return false;
    }

    std::string error;

    if (!dispatcher_.Attach(
            native_invoker_,
            error)) {
        return false;
    }

    ready_.store(
        dispatcher_.Attached(),
        std::memory_order_release);

    return true;
}

void Rdr1GameTickSource::Bootstrap() {
    while (running_.load(std::memory_order_acquire)) {
        if (!dispatcher_.Attached() && InitializeRdr1()) {
            recurring_task_ =
                std::make_shared<std::function<void()>>();

            const auto weak_task =
                std::weak_ptr<std::function<void()>>(recurring_task_);

            *recurring_task_ =
                [this, weak_task]() {
                    if (!running_.load(std::memory_order_acquire)) {
                        return;
                    }

                    if (callback_) {
                        callback_();
                    }

                    if (running_.load(std::memory_order_acquire) &&
                        dispatcher_.Attached()) {
                        if (const auto task = weak_task.lock()) {
                            std::string ignored;
                            dispatcher_.Submit(*task, ignored);
                        }
                    }
                };

            std::string error;
            if (!dispatcher_.Submit(*recurring_task_, error)) {
                recurring_task_.reset();
                dispatcher_.Detach();
                ready_.store(false, std::memory_order_release);
            } else {
                ready_.store(true, std::memory_order_release);
                return;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
    }
}

}

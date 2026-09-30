#include "GameBridge.hpp"

#include "rdr1/PatternScanner.hpp"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "../../shared/logging/Logger.hpp"

namespace dustwire::game {

namespace {

constexpr const char* kLocalPlayerPattern =
    "48 89 15 ? ? ? ? E9 ? ? ? ?";

constexpr const char* kActorManagerPattern =
    "48 8B 05 ? ? ? ? 0F B7 CA 48 03 C9 C1 EA 10 66 39 54 C8 ? 75 03 B0 01 C3 32 C0 C3 CC 48 89 5C 24 ?";

struct MinimalSagPlayer final {
    std::byte padding0[0x5EC];
    std::uint32_t guid;
};

struct MinimalSagActor final {
    std::byte padding0[0xB0];
    std::uintptr_t actor_component;
};

struct MinimalSagActorComponent final {
    std::byte padding0[0x18];
    std::uintptr_t transform;
};

struct MinimalMatrix34 final {
    std::byte padding0[0x30];
    Vec3 position;
};

std::uintptr_t ResolveRipTarget(std::uintptr_t instruction) noexcept {
    if (!instruction) {
        return 0;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            reinterpret_cast<const void*>(instruction + 3),
            &mbi,
            sizeof(mbi)) != sizeof(mbi)) {
        return 0;
    }

    std::int32_t displacement = 0;

    __try {
        std::memcpy(
            &displacement,
            reinterpret_cast<const void*>(instruction + 3),
            sizeof(displacement));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    return instruction +
           7 +
           static_cast<std::intptr_t>(displacement);
}

bool Readable(std::uintptr_t address, std::size_t size) noexcept {
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

bool GuardedCopy(
    const void* source,
    void* destination,
    std::size_t size) noexcept {

    if (!source || !destination || !size) {
        return false;
    }

    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

bool GameBridge::ReadBytes(
    std::uintptr_t address,
    void* out,
    std::size_t size) const {

    if (!Readable(address, size)) {
        return false;
    }

    return GuardedCopy(
        reinterpret_cast<const void*>(address),
        out,
        size);
}

bool GameBridge::ReadPointer(
    std::uintptr_t address,
    std::uintptr_t& out) const {

    out = 0;

    if (!ReadBytes(
            address,
            &out,
            sizeof(out))) {
        return false;
    }

    return out != 0;
}

bool GameBridge::Initialize() {
    auto& logger =
        dustwire::logging::Logger::Instance();

    local_player_ = 0;
    local_player_ready_ = false;
    local_player_guid_ = 0;
    local_player_position_ = {};

#ifdef _WIN32
    const auto module =
        GetModuleHandleW(nullptr);

    if (!module) {
        logger.Error(
            "RDR1 GameBridge: main module not available");
        return false;
    }

    module_base_ =
        reinterpret_cast<std::uintptr_t>(module);

    const auto* dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(
            module_base_);

    if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0) {
        logger.Error(
            "RDR1 GameBridge: invalid DOS header");
        return false;
    }

    const auto* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            module_base_ +
            static_cast<std::uintptr_t>(
                dos->e_lfanew));

    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine !=
            IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic !=
            IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        logger.Error(
            "RDR1 GameBridge: unsupported PE layout");
        return false;
    }

    image_size_ =
        nt->OptionalHeader.SizeOfImage;

    const auto* text =
        IMAGE_FIRST_SECTION(
            const_cast<IMAGE_NT_HEADERS64*>(nt));

    std::uint32_t text_rva = 0;
    std::size_t text_size = 0;

    for (WORD i = 0;
         i < nt->FileHeader.NumberOfSections;
         ++i) {
        char name[9]{};
        std::memcpy(
            name,
            text[i].Name,
            8);

        if (std::string(name) == ".text") {
            text_rva =
                text[i].VirtualAddress;
            text_size =
                text[i].Misc.VirtualSize;
            break;
        }
    }

    if (!text_rva || !text_size) {
        logger.Error(
            "RDR1 GameBridge: .text section not found");
        return false;
    }

    const auto text_base =
        reinterpret_cast<const std::uint8_t*>(
            module_base_ + text_rva);

    const auto local_pattern =
        dustwire::rdr1::BytePattern::Parse(
            kLocalPlayerPattern);
    const auto actor_pattern =
        dustwire::rdr1::BytePattern::Parse(
            kActorManagerPattern);

    if (!local_pattern || !actor_pattern) {
        logger.Error(
            "RDR1 GameBridge: pattern parse failed");
        return false;
    }

    const auto local_hit =
        dustwire::rdr1::PatternScanner::ScanBuffer(
            text_base,
            text_size,
            *local_pattern);

    const auto actor_hit =
        dustwire::rdr1::PatternScanner::ScanBuffer(
            text_base,
            text_size,
            *actor_pattern);

    if (!local_hit || !actor_hit) {
        logger.Error(
            "RDR1 GameBridge: required RDR1 actor patterns not found");
        return false;
    }

    local_player_storage_ =
        ResolveRipTarget(*local_hit);
    actor_manager_slots_storage_ =
        ResolveRipTarget(*actor_hit);

    if (!dustwire::rdr1::PatternScanner::ValidateInModule(
            local_player_storage_,
            module_base_,
            image_size_) ||
        !Readable(
            actor_manager_slots_storage_,
            sizeof(std::uintptr_t))) {
        local_player_storage_ = 0;
        actor_manager_slots_storage_ = 0;
        logger.Error(
            "RDR1 GameBridge: actor symbols resolved outside expected layout");
        return false;
    }

    logger.Info(
        "RDR1 GameBridge actor symbols resolved");

    return true;
#else
    logger.Error(
        "RDR1 GameBridge is Windows-only");
    return false;
#endif
}

bool GameBridge::ReadLocalPlayerPosition(
    Vec3& out_position,
    std::uint32_t& out_guid) const {

    out_position = {};
    out_guid = 0;

    std::uintptr_t local_player = 0;
    std::uintptr_t manager_slots = 0;

    if (!ReadPointer(
            local_player_storage_,
            local_player)) {
        return false;
    }

    if (!ReadPointer(
            actor_manager_slots_storage_,
            manager_slots)) {
        return false;
    }

    MinimalSagPlayer player{};
    if (!ReadBytes(
            local_player,
            &player,
            sizeof(player))) {
        return false;
    }

    const std::uint32_t guid =
        player.guid;

    const auto actor_slot_address =
        manager_slots +
        (static_cast<std::uintptr_t>(
             static_cast<std::uint16_t>(guid)) *
         0x10u);

    std::uintptr_t actor = 0;

    if (!ReadPointer(
            actor_slot_address,
            actor)) {
        return false;
    }

    MinimalSagActor sag_actor{};

    if (!ReadBytes(
            actor,
            &sag_actor,
            sizeof(sag_actor))) {
        return false;
    }

    if (!sag_actor.actor_component) {
        return false;
    }

    MinimalSagActorComponent component{};

    if (!ReadBytes(
            sag_actor.actor_component,
            &component,
            sizeof(component))) {
        return false;
    }

    if (!component.transform) {
        return false;
    }

    MinimalMatrix34 matrix{};

    if (!ReadBytes(
            component.transform,
            &matrix,
            sizeof(matrix))) {
        return false;
    }

    out_guid = guid;
    out_position = matrix.position;
    return true;
}

void GameBridge::Tick() {
    Vec3 position{};
    std::uint32_t guid = 0;

    if (!ReadLocalPlayerPosition(
            position,
            guid)) {
        return;
    }

    const bool first_resolution =
        !local_player_ready_;

    local_player_guid_ = guid;
    local_player_position_ = position;
    local_player_ready_ = true;

    // Keep the public local ActorId stable while preserving the game's
    // actual actor GUID separately for later replication/protocol mapping.
    if (local_player_ == 0) {
        local_player_ = 1;
    }

    if (first_resolution) {
        auto& logger =
            dustwire::logging::Logger::Instance();

        logger.Info(
            "RDR1 local actor resolved; guid=" +
            std::to_string(local_player_guid_));
    }
}

ActorId GameBridge::CreateRemotePlayer(
    std::uint32_t,
    Vec3,
    Vec3) {

    const auto actor = next_actor_id_++;

    dustwire::logging::Logger::Instance().Info(
        "RDR1 remote actor requested id=" +
        std::to_string(actor));

    return actor;
}

void GameBridge::DeleteRemotePlayer(
    ActorId actor) {

    dustwire::logging::Logger::Instance().Info(
        "RDR1 remote actor delete requested id=" +
        std::to_string(actor));
}

void GameBridge::SetTransform(
    ActorId actor,
    Vec3,
    Vec3) {

    dustwire::logging::Logger::Instance().Info(
        "RDR1 remote actor transform requested id=" +
        std::to_string(actor));
}

} // namespace dustwire::game

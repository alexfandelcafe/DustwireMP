#pragma once
#include <cstdint>
#include <string>

namespace dustwire::game {

struct Vec3 {
    float x{}, y{}, z{};
};

using ActorId = std::uint32_t;

class GameBridge {
public:
    bool Initialize();
    void Tick();
    ActorId LocalPlayer() const { return local_player_; }
    bool LocalPlayerReady() const { return local_player_ready_; }
    Vec3 LocalPlayerPosition() const { return local_player_position_; }

    // v0.1 stubs. These are the only functions networking/gameplay should call.
    ActorId CreateRemotePlayer(std::uint32_t model, Vec3 position, Vec3 rotation);
    void DeleteRemotePlayer(ActorId actor);
    void SetTransform(ActorId actor, Vec3 position, Vec3 rotation);

private:
    bool ReadPointer(std::uintptr_t address, std::uintptr_t& out) const;
    bool ReadBytes(std::uintptr_t address, void* out, std::size_t size) const;
    bool ReadLocalPlayerPosition(Vec3& out_position, std::uint32_t& out_guid) const;

    std::uintptr_t module_base_{};
    std::size_t image_size_{};
    std::uintptr_t local_player_storage_{};
    std::uintptr_t actor_manager_slots_storage_{};

    ActorId local_player_{0};
    ActorId next_actor_id_{1};
    bool local_player_ready_{};
    Vec3 local_player_position_{};
    std::uint32_t local_player_guid_{};
};

}

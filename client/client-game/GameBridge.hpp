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

    // v0.1 stubs. These are the only functions networking/gameplay should call.
    ActorId CreateRemotePlayer(std::uint32_t model, Vec3 position, Vec3 rotation);
    void DeleteRemotePlayer(ActorId actor);
    void SetTransform(ActorId actor, Vec3 position, Vec3 rotation);

private:
    ActorId local_player_{0};
    ActorId next_actor_id_{1};
};

}

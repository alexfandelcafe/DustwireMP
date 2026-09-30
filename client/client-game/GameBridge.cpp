#include "GameBridge.hpp"
#include <iostream>

namespace dustwire::game {

bool GameBridge::Initialize() {
    std::cout << "[GameBridge] initialized (standalone stub)\n";
    local_player_ = 0;
    return true;
}

void GameBridge::Tick() {}

ActorId GameBridge::CreateRemotePlayer(std::uint32_t, Vec3, Vec3) {
    const auto actor = next_actor_id_++;
    std::cout << "[GameBridge] create remote actor " << actor << "\n";
    return actor;
}

void GameBridge::DeleteRemotePlayer(ActorId actor) {
    std::cout << "[GameBridge] delete remote actor " << actor << "\n";
}

void GameBridge::SetTransform(ActorId actor, Vec3, Vec3) {
    std::cout << "[GameBridge] set transform actor " << actor << "\n";
}

}

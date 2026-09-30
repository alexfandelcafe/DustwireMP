#include "GameTick.hpp"

namespace dustwire::client {

bool BootstrapTickSource::Start(
    std::function<void()> callback) {

    if (running_.exchange(true)) {
        return false;
    }

    if (!callback) {
        running_ = false;
        return false;
    }

    worker_ =
        std::thread(
            [this, callback = std::move(callback)]() mutable {
                while (running_) {
                    callback();
                    std::this_thread::sleep_for(interval_);
                }
            });

    return true;
}

void BootstrapTickSource::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

}

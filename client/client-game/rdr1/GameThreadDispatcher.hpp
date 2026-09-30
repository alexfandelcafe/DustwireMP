#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace dustwire::rdr1 {

class NativeInvoker;

class GameThreadDispatcher final {
public:
    using Handler = void(*)(void* context);

    bool Attach(
        NativeInvoker& invoker,
        std::string& error);

    void Detach();

    bool Attached() const {
        return attached_.load(
            std::memory_order_acquire);
    }

    bool GameThreadKnown() const {
        return game_thread_known_.load(
            std::memory_order_acquire);
    }

    bool Submit(
        std::function<void()> task,
        std::string& error);

    bool SubmitAndWait(
        std::function<void()> task,
        std::uint32_t timeout_ms,
        std::string& error);

    std::size_t Pump(
        std::size_t max_tasks = 32,
        void* wait_context = nullptr);

    bool IsGameThread() const;

private:
    struct PendingTask final {
        std::function<void()> fn;
        std::mutex mutex;
        std::condition_variable cv;
        bool completed{};
        bool cancelled{};
    };

    static void WaitHook(void* context);
    static void GetThisScriptIdHook(void* context);
    static void GetScriptNameHook(void* context);

    bool IsAuthorizedWaitContext(
        void* context) const;

    NativeInvoker* invoker_{};
    std::atomic<Handler> original_wait_{};
    std::atomic<Handler> original_get_this_script_id_{};
    std::atomic<Handler> original_get_script_name_{};
    std::atomic<bool> attached_{false};

    mutable std::mutex queue_mutex_;
    std::deque<std::shared_ptr<PendingTask>> queue_;

    std::thread::id game_thread_id_{};
    std::uintptr_t game_thread_context_{};
    std::atomic<bool> game_thread_known_{false};
};

}

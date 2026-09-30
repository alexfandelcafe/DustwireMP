#include "GameThreadDispatcher.hpp"

#include "NativeInvoker.hpp"

#include <Windows.h>
#include <intrin.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "../../../shared/logging/Logger.hpp"

namespace dustwire::rdr1 {

namespace {

constexpr std::uint32_t kNativeScrThreadWait = 0x7715C03Bu;
constexpr std::uint32_t kNativeGetThisScriptId = 0x9C424E0Du;
constexpr std::uint32_t kNativeGetScriptName = 0x0BC52445u;

std::atomic<GameThreadDispatcher*> g_dispatcher{nullptr};
std::atomic<std::uint32_t> g_hook_in_flight{0};
std::atomic<NativeInvoker::NativeHandler> g_fallback_original_wait{nullptr};
std::atomic<NativeInvoker::NativeHandler> g_fallback_original_script_id{nullptr};
std::atomic<NativeInvoker::NativeHandler> g_fallback_original_script_name{nullptr};

struct NativeContext final {
    void* return_buffer{};
    std::uint32_t argument_count{};
    void* argument_buffer{};
    std::uint32_t data_count{};
    void* output_vectors[4]{};
    std::uint8_t input_vectors[0x30]{};
    std::uintptr_t stack[32]{};
};

struct ScriptContext final {
    std::uintptr_t context{};
    char name[96]{};
    bool used{};
};

std::mutex g_context_mutex;
ScriptContext g_contexts[64]{};
std::size_t g_context_next{};

bool ReadU32Return(
    void* context,
    std::uint32_t& value) noexcept {

    value = 0;

    if (!context) {
        return false;
    }

    const auto* native =
        reinterpret_cast<const NativeContext*>(context);

    if (!native->return_buffer) {
        return false;
    }

    __try {
        std::memcpy(
            &value,
            native->return_buffer,
            sizeof(value));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadPointerReturn(
    void* context,
    std::uintptr_t& value) noexcept {

    value = 0;

    if (!context) {
        return false;
    }

    const auto* native =
        reinterpret_cast<const NativeContext*>(context);

    if (!native->return_buffer) {
        return false;
    }

    __try {
        std::memcpy(
            &value,
            native->return_buffer,
            sizeof(value));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadCString(
    const void* pointer,
    char* output,
    std::size_t capacity) noexcept {

    if (!pointer ||
        !output ||
        capacity == 0) {
        return false;
    }

    output[0] = '\0';

    __try {
        const auto* source =
            reinterpret_cast<const char*>(pointer);

        for (std::size_t i = 0;
             i + 1 < capacity;
             ++i) {

            const auto ch = source[i];

            if (ch == '\0') {
                return true;
            }

            if (static_cast<unsigned char>(ch) < 0x20u ||
                static_cast<unsigned char>(ch) > 0x7Eu) {
                return false;
            }

            output[i] = ch;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        output[0] = '\0';
        return false;
    }

    output[capacity - 1] = '\0';
    return true;
}

void RecordScriptName(
    void* context,
    const char* name) {

    if (!context || !name || !*name) {
        return;
    }

    const auto key =
        reinterpret_cast<std::uintptr_t>(context);

    std::lock_guard lock(g_context_mutex);

    for (auto& entry : g_contexts) {
        if (entry.used &&
            entry.context == key) {

            std::strncpy(
                entry.name,
                name,
                sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            return;
        }
    }

    for (auto& entry : g_contexts) {
        if (!entry.used) {
            entry.used = true;
            entry.context = key;
            std::strncpy(
                entry.name,
                name,
                sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            return;
        }
    }

    auto& entry =
        g_contexts[
            g_context_next++ %
            (sizeof(g_contexts) /
             sizeof(g_contexts[0]))];

    entry.used = true;
    entry.context = key;
    std::strncpy(
        entry.name,
        name,
        sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
}

void RecordScriptId(
    void* context,
    std::uint32_t script_id) {

    if (!context) {
        return;
    }

    // The ID is intentionally not used for authorization. Keep the call
    // observed for parity with the historical dispatcher.
    (void)script_id;
}

bool LookupScriptName(
    void* context,
    char* output,
    std::size_t capacity) {

    if (!context ||
        !output ||
        capacity == 0) {
        return false;
    }

    output[0] = '\0';

    const auto key =
        reinterpret_cast<std::uintptr_t>(context);

    std::lock_guard lock(g_context_mutex);

    for (const auto& entry : g_contexts) {
        if (entry.used &&
            entry.context == key &&
            entry.name[0] != '\0') {

            std::strncpy(
                output,
                entry.name,
                capacity - 1);
            output[capacity - 1] = '\0';
            return true;
        }
    }

    return false;
}

}

bool GameThreadDispatcher::Attach(
    NativeInvoker& invoker,
    std::string& error) {

    error.clear();

    if (attached_.load(std::memory_order_acquire)) {
        return true;
    }

    if (g_dispatcher.load(std::memory_order_acquire) != nullptr) {
        error = "another RDR1 game-thread dispatcher is attached";
        return false;
    }

    NativeInvoker::NativeHandler wait = nullptr;
    NativeInvoker::NativeHandler script_id = nullptr;
    NativeInvoker::NativeHandler script_name = nullptr;

    if (!invoker.CurrentHandler(
            kNativeScrThreadWait,
            wait) ||
        !invoker.CurrentHandler(
            kNativeGetThisScriptId,
            script_id) ||
        !invoker.CurrentHandler(
            kNativeGetScriptName,
            script_name)) {
        error = "historical RDR1 game-thread native set is incomplete";
        return false;
    }

    invoker_ = &invoker;
    original_wait_.store(
        wait,
        std::memory_order_release);
    original_get_this_script_id_.store(
        script_id,
        std::memory_order_release);
    original_get_script_name_.store(
        script_name,
        std::memory_order_release);

    g_fallback_original_wait.store(
        wait,
        std::memory_order_release);
    g_fallback_original_script_id.store(
        script_id,
        std::memory_order_release);
    g_fallback_original_script_name.store(
        script_name,
        std::memory_order_release);

    attached_.store(true, std::memory_order_release);
    game_thread_known_.store(false, std::memory_order_release);
    g_dispatcher.store(
        this,
        std::memory_order_release);

    NativeInvoker::NativeHandler installed = nullptr;

    if (!invoker.HookNative(
            kNativeScrThreadWait,
            &GameThreadDispatcher::WaitHook,
            installed,
            &error) ||
        installed != wait) {

        if (installed) {
            invoker.UnhookNative(
                kNativeScrThreadWait,
                &GameThreadDispatcher::WaitHook,
                installed);
        }

        g_dispatcher.store(
            nullptr,
            std::memory_order_release);
        attached_.store(
            false,
            std::memory_order_release);
        invoker_ = nullptr;
        original_wait_.store(nullptr);
        return false;
    }

    if (!invoker.HookNative(
            kNativeGetThisScriptId,
            &GameThreadDispatcher::GetThisScriptIdHook,
            installed,
            &error) ||
        installed != script_id) {

        invoker.UnhookNative(
            kNativeScrThreadWait,
            &GameThreadDispatcher::WaitHook,
            wait);

        g_dispatcher.store(nullptr);
        attached_.store(false);
        invoker_ = nullptr;
        original_wait_.store(nullptr);
        return false;
    }

    if (!invoker.HookNative(
            kNativeGetScriptName,
            &GameThreadDispatcher::GetScriptNameHook,
            installed,
            &error) ||
        installed != script_name) {

        invoker.UnhookNative(
            kNativeGetThisScriptId,
            &GameThreadDispatcher::GetThisScriptIdHook,
            script_id);
        invoker.UnhookNative(
            kNativeScrThreadWait,
            &GameThreadDispatcher::WaitHook,
            wait);

        g_dispatcher.store(nullptr);
        attached_.store(false);
        invoker_ = nullptr;
        original_wait_.store(nullptr);
        return false;
    }

    dustwire::logging::Logger::Instance().Info(
        "RDR1 dispatcher hooks installed: scrThread::Wait, GET_THIS_SCRIPT_ID, GET_SCRIPT_NAME");

    OutputDebugStringA(
        "[DustwireRDR1] game-thread dispatcher attached\n");
    return true;
}

void GameThreadDispatcher::Detach() {
    if (!attached_.load(std::memory_order_acquire)) {
        return;
    }

    std::deque<std::shared_ptr<PendingTask>> cancelled;

    {
        std::lock_guard lock(queue_mutex_);
        cancelled.swap(queue_);
    }

    for (const auto& task : cancelled) {
        {
            std::lock_guard lock(task->mutex);
            task->cancelled = true;
            task->completed = true;
        }
        task->cv.notify_one();
    }

    if (invoker_) {
        invoker_->UnhookNative(
            kNativeGetScriptName,
            &GameThreadDispatcher::GetScriptNameHook,
            original_get_script_name_.load(
                std::memory_order_acquire));
        invoker_->UnhookNative(
            kNativeGetThisScriptId,
            &GameThreadDispatcher::GetThisScriptIdHook,
            original_get_this_script_id_.load(
                std::memory_order_acquire));
        invoker_->UnhookNative(
            kNativeScrThreadWait,
            &GameThreadDispatcher::WaitHook,
            original_wait_.load(
                std::memory_order_acquire));
    }

    if (g_dispatcher.load(
            std::memory_order_acquire) == this) {
        g_dispatcher.store(
            nullptr,
            std::memory_order_release);
    }

    while (g_hook_in_flight.load(
               std::memory_order_acquire) != 0) {
        std::this_thread::yield();
    }

    g_fallback_original_wait.store(nullptr);
    g_fallback_original_script_id.store(nullptr);
    g_fallback_original_script_name.store(nullptr);

    invoker_ = nullptr;
    original_wait_.store(nullptr);
    original_get_this_script_id_.store(nullptr);
    original_get_script_name_.store(nullptr);
    attached_.store(false, std::memory_order_release);
    game_thread_known_.store(false, std::memory_order_release);
    game_thread_id_ = {};
    game_thread_context_ = 0;
}

bool GameThreadDispatcher::Submit(
    std::function<void()> task,
    std::string& error) {

    error.clear();

    if (!Attached()) {
        error = "RDR1 game-thread dispatcher not attached";
        return false;
    }

    if (!task) {
        error = "empty RDR1 game-thread task";
        return false;
    }

    auto pending =
        std::make_shared<PendingTask>();

    pending->fn = std::move(task);

    {
        std::lock_guard lock(queue_mutex_);

        if (!Attached()) {
            error = "RDR1 game-thread dispatcher detached";
            return false;
        }

        queue_.push_back(std::move(pending));
    }

    return true;
}

bool GameThreadDispatcher::SubmitAndWait(
    std::function<void()> task,
    std::uint32_t timeout_ms,
    std::string& error) {

    error.clear();

    if (!Attached()) {
        error = "RDR1 game-thread dispatcher not attached";
        return false;
    }

    if (!task) {
        error = "empty RDR1 game-thread task";
        return false;
    }

    if (IsGameThread()) {
        task();
        return true;
    }

    auto pending =
        std::make_shared<PendingTask>();

    pending->fn = std::move(task);

    {
        std::lock_guard lock(queue_mutex_);

        if (!Attached()) {
            error = "RDR1 game-thread dispatcher detached";
            return false;
        }

        queue_.push_back(pending);
    }

    std::unique_lock task_lock(pending->mutex);

    const bool signalled =
        pending->cv.wait_for(
            task_lock,
            std::chrono::milliseconds(timeout_ms),
            [&pending] {
                return pending->completed;
            });

    if (!signalled) {
        bool removed = false;

        {
            std::lock_guard queue_lock(queue_mutex_);

            for (auto it = queue_.begin();
                 it != queue_.end();
                 ++it) {
                if (it->get() == pending.get()) {
                    queue_.erase(it);
                    removed = true;
                    break;
                }
            }
        }

        if (removed) {
            {
                std::lock_guard lock(pending->mutex);

                if (!pending->completed) {
                    pending->cancelled = true;
                    pending->completed = true;
                }
            }

            pending->cv.notify_one();
            error = "RDR1 game-thread task timed out";
            return false;
        }

        pending->cv.wait(
            task_lock,
            [&pending] {
                return pending->completed;
            });

        if (pending->cancelled) {
            error = "RDR1 game-thread task completed after timeout";
            return false;
        }

        return true;
    }

    if (pending->cancelled) {
        error = "RDR1 game-thread task cancelled";
        return false;
    }

    return true;
}

std::size_t GameThreadDispatcher::Pump(
    std::size_t max_tasks,
    void* wait_context) {

    if (!Attached() ||
        max_tasks == 0 ||
        wait_context == nullptr) {
        return 0;
    }

    if (!IsAuthorizedWaitContext(wait_context)) {
        return 0;
    }

    if (!game_thread_known_.load(
            std::memory_order_acquire)) {

        std::lock_guard lock(queue_mutex_);

        if (!game_thread_known_.load(
                std::memory_order_relaxed)) {

            game_thread_id_ =
                std::this_thread::get_id();

            game_thread_context_ =
                reinterpret_cast<std::uintptr_t>(
                    wait_context);

            game_thread_known_.store(
                true,
                std::memory_order_release);

            dustwire::logging::Logger::Instance().Info(
                "RDR1 game-thread context acquired");
        }
    }

    std::size_t processed = 0;

    while (processed < max_tasks) {
        std::shared_ptr<PendingTask> task;

        {
            std::lock_guard lock(queue_mutex_);

            if (queue_.empty()) {
                break;
            }

            task = std::move(queue_.front());
            queue_.pop_front();
        }

        bool cancelled = false;

        {
            std::lock_guard lock(task->mutex);
            cancelled = task->cancelled;
        }

        if (!cancelled) {
            try {
                task->fn();
            } catch (...) {
                // Never let a client exception escape into the RDR1 native path.
            }
        }

        {
            std::lock_guard lock(task->mutex);
            task->completed = true;
        }

        task->cv.notify_one();
        ++processed;
    }

    return processed;
}

bool GameThreadDispatcher::IsAuthorizedWaitContext(
    void* context) const {

    char name[96]{};

    if (!LookupScriptName(
            context,
            name,
            sizeof(name))) {
        return false;
    }

    std::string normalized{name};

    for (char& ch : normalized) {
        if (ch == '\\') {
            ch = '/';
        }

        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(
                ch - 'A' + 'a');
        }
    }

    return normalized == "content/main" ||
           normalized == "content/pressstart" ||
           normalized == "main" ||
           normalized == "pressstart" ||
           normalized == "content/main.sc" ||
           normalized == "content/pressstart.sc" ||
           normalized == "main.sc" ||
           normalized == "pressstart.sc";
}

bool GameThreadDispatcher::IsGameThread() const {
    return game_thread_known_.load(
               std::memory_order_acquire) &&
           std::this_thread::get_id() ==
               game_thread_id_;
}

void GameThreadDispatcher::WaitHook(
    void* context) {

    static std::atomic<std::uint32_t> trace_count{0};
    const auto trace = trace_count.fetch_add(
        1,
        std::memory_order_relaxed);

    if (trace < 16) {
        char line[192]{};
        std::snprintf(
            line,
            sizeof(line),
            "RDR1 scrThread::Wait hook trace=%u context=0x%llX tid=%lu",
            trace,
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(context)),
            static_cast<unsigned long>(
                GetCurrentThreadId()));

        dustwire::logging::Logger::Instance().Info(
            line);
    }

    g_hook_in_flight.fetch_add(
        1,
        std::memory_order_acq_rel);

    struct Guard final {
        ~Guard() {
            g_hook_in_flight.fetch_sub(
                1,
                std::memory_order_release);
        }
    } guard{};

    auto* dispatcher =
        g_dispatcher.load(
            std::memory_order_acquire);

    if (dispatcher) {
        dispatcher->Pump(32, context);

        const auto original =
            dispatcher->original_wait_.load(
                std::memory_order_acquire);

        if (original) {
            original(context);
        }

        return;
    }

    const auto original =
        g_fallback_original_wait.load(
            std::memory_order_acquire);

    if (original) {
        original(context);
    }
}

void GameThreadDispatcher::GetThisScriptIdHook(
    void* context) {

    g_hook_in_flight.fetch_add(
        1,
        std::memory_order_acq_rel);

    struct Guard final {
        ~Guard() {
            g_hook_in_flight.fetch_sub(
                1,
                std::memory_order_release);
        }
    } guard{};

    auto* dispatcher =
        g_dispatcher.load(
            std::memory_order_acquire);

    const auto original =
        dispatcher != nullptr
            ? dispatcher->original_get_this_script_id_.load(
                  std::memory_order_acquire)
            : g_fallback_original_script_id.load(
                  std::memory_order_acquire);

    if (!original) {
        return;
    }

    original(context);

    std::uint32_t script_id = 0;

    if (ReadU32Return(context, script_id)) {
        RecordScriptId(context, script_id);
    }
}

void GameThreadDispatcher::GetScriptNameHook(
    void* context) {

    g_hook_in_flight.fetch_add(
        1,
        std::memory_order_acq_rel);

    struct Guard final {
        ~Guard() {
            g_hook_in_flight.fetch_sub(
                1,
                std::memory_order_release);
        }
    } guard{};

    auto* dispatcher =
        g_dispatcher.load(
            std::memory_order_acquire);

    const auto original =
        dispatcher != nullptr
            ? dispatcher->original_get_script_name_.load(
                  std::memory_order_acquire)
            : g_fallback_original_script_name.load(
                  std::memory_order_acquire);

    if (!original) {
        return;
    }

    original(context);

    std::uintptr_t pointer = 0;
    char name[96]{};

    if (ReadPointerReturn(
            context,
            pointer) &&
        pointer != 0 &&
        ReadCString(
            reinterpret_cast<const void*>(pointer),
            name,
            sizeof(name))) {

        RecordScriptName(context, name);
    }
}

}

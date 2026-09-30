#pragma once

#include <filesystem>
#include <mutex>
#include <string>

namespace dustwire::logging {

enum class Level {
    Info,
    Warning,
    Error,
};

class Logger {
public:
    static Logger& Instance();

    void Initialize(
        const std::filesystem::path& directory,
        const std::string& component);

    void Log(
        Level level,
        const std::string& message);

    void Info(const std::string& message) {
        Log(Level::Info, message);
    }

    void Warning(const std::string& message) {
        Log(Level::Warning, message);
    }

    void Error(const std::string& message) {
        Log(Level::Error, message);
    }

private:
    Logger() = default;

    std::mutex mutex_;
    std::filesystem::path file_;
    std::string component_{"DustwireMP"};
};

}

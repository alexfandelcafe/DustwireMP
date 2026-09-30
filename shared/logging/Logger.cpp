#include "Logger.hpp"

#include <windows.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dustwire::logging {

namespace {

const char* LevelName(Level level) {
    switch (level) {
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARN";
        case Level::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}

std::string Timestamp() {
    const auto now =
        std::chrono::system_clock::now();

    const std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    localtime_s(&tm, &time);

    std::ostringstream stream;
    stream << std::put_time(
        &tm,
        "%Y-%m-%d %H:%M:%S");

    return stream.str();
}

}

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

void Logger::Initialize(
    const std::filesystem::path& directory,
    const std::string& component) {

    std::lock_guard lock(mutex_);

    component_ = component;

    std::error_code error;
    std::filesystem::create_directories(
        directory,
        error);

    file_ =
        directory /
        (component_ + ".log");
}

void Logger::Log(
    Level level,
    const std::string& message) {

    std::lock_guard lock(mutex_);

    const std::string line =
        "[" + Timestamp() + "] [" +
        LevelName(level) + "] [" +
        component_ + "] " +
        message + "\n";

    OutputDebugStringA(line.c_str());

    if (file_.empty()) {
        return;
    }

    std::ofstream out(
        file_,
        std::ios::app);

    if (out) {
        out << line;
    }
}

}

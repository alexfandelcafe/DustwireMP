#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dustwire::launcher {

struct ProcessInfo {
    std::uint32_t pid{};
    std::wstring exe_name;
    std::wstring image_path;
};

class ProcessLocator {
public:
    std::vector<ProcessInfo> FindByName(
        const std::wstring& exe_name) const;

    ProcessInfo FindFirstByName(
        const std::wstring& exe_name) const;

    bool Launch(
        const std::wstring& executable,
        const std::wstring& working_directory,
        std::uint32_t& process_id) const;

    bool WaitForProcess(
        const std::wstring& exe_name,
        std::uint32_t timeout_ms,
        ProcessInfo& result) const;
};

}

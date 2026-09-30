#include "ProcessLocator.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <chrono>
#include <thread>
#include <utility>

namespace dustwire::launcher {

std::vector<ProcessInfo> ProcessLocator::FindByName(
    const std::wstring& exe_name) const {

    std::vector<ProcessInfo> result;

    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(
            snapshot,
            &entry)) {

        do {
            if (_wcsicmp(
                    entry.szExeFile,
                    exe_name.c_str()) != 0) {
                continue;
            }

            ProcessInfo info;
            info.pid =
                entry.th32ProcessID;
            info.exe_name =
                entry.szExeFile;

            HANDLE process =
                OpenProcess(
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE,
                    info.pid);

            if (process != nullptr) {
                wchar_t path[MAX_PATH * 4]{};
                DWORD path_length =
                    static_cast<DWORD>(
                        sizeof(path) /
                        sizeof(path[0]));

                if (QueryFullProcessImageNameW(
                        process,
                        0,
                        path,
                        &path_length)) {

                    info.image_path.assign(
                        path,
                        path_length);
                }

                CloseHandle(process);
            }

            result.push_back(
                std::move(info));

        } while (Process32NextW(
            snapshot,
            &entry));
    }

    CloseHandle(snapshot);
    return result;
}

ProcessInfo ProcessLocator::FindFirstByName(
    const std::wstring& exe_name) const {

    const auto matches =
        FindByName(exe_name);

    if (matches.empty()) {
        return {};
    }

    return matches.front();
}

bool ProcessLocator::Launch(
    const std::wstring& executable,
    const std::wstring& working_directory,
    std::uint32_t& process_id) const {

    std::wstring command_line =
        L"\"" + executable + L"\"";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            executable.c_str(),
            command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            working_directory.empty()
                ? nullptr
                : working_directory.c_str(),
            &startup,
            &process)) {

        return false;
    }

    process_id =
        process.dwProcessId;

    CloseHandle(
        process.hThread);
    CloseHandle(
        process.hProcess);

    return true;
}

bool ProcessLocator::WaitForProcess(
    const std::wstring& exe_name,
    std::uint32_t timeout_ms,
    ProcessInfo& result) const {

    constexpr std::uint32_t poll_ms = 100;

    const auto start =
        std::chrono::steady_clock::now();

    for (;;) {
        result =
            FindFirstByName(exe_name);

        if (result.pid != 0) {
            return true;
        }

        const auto elapsed =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::steady_clock::now() -
                start).count();

        if (elapsed >= timeout_ms) {
            return false;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                poll_ms));
    }
}

}

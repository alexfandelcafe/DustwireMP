#include "Injector.hpp"

#include <windows.h>

#include <sstream>
#include <string>

namespace dustwire::launcher {

namespace {

std::string Win32Error(
    const char* operation,
    DWORD code) {

    std::ostringstream stream;
    stream << operation
           << " failed with Win32 error "
           << code;
    return stream.str();
}

class HandleGuard {
public:
    explicit HandleGuard(HANDLE handle)
        : handle_(handle) {}

    ~HandleGuard() {
        if (handle_ != nullptr &&
            handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    HANDLE get() const {
        return handle_;
    }

    HANDLE release() {
        HANDLE value = handle_;
        handle_ = nullptr;
        return value;
    }

private:
    HANDLE handle_;
};

}

InjectionResult Injector::Inject(
    std::uint32_t process_id,
    const std::wstring& dll_path) const {

    InjectionResult result;

    if (dll_path.empty()) {
        result.error = "DLL path is empty";
        return result;
    }

    HANDLE raw_process = OpenProcess(
        PROCESS_CREATE_THREAD |
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE |
        PROCESS_VM_READ,
        FALSE,
        process_id);

    if (raw_process == nullptr) {
        result.error = Win32Error(
            "OpenProcess",
            GetLastError());
        return result;
    }

    HandleGuard process(raw_process);

    const SIZE_T bytes =
        (dll_path.size() + 1) * sizeof(wchar_t);

    void* remote_memory = VirtualAllocEx(
        process.get(),
        nullptr,
        bytes,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);

    if (remote_memory == nullptr) {
        result.error = Win32Error(
            "VirtualAllocEx",
            GetLastError());
        return result;
    }

    const auto cleanup_memory =
        [&]() {
            VirtualFreeEx(
                process.get(),
                remote_memory,
                0,
                MEM_RELEASE);
        };

    SIZE_T written = 0;

    if (!WriteProcessMemory(
            process.get(),
            remote_memory,
            dll_path.c_str(),
            bytes,
            &written) ||
        written != bytes) {

        result.error = Win32Error(
            "WriteProcessMemory",
            GetLastError());

        cleanup_memory();
        return result;
    }

    HMODULE kernel32 =
        GetModuleHandleW(L"kernel32.dll");

    if (kernel32 == nullptr) {
        result.error = Win32Error(
            "GetModuleHandleW(kernel32.dll)",
            GetLastError());

        cleanup_memory();
        return result;
    }

    auto load_library =
        reinterpret_cast<
            LPTHREAD_START_ROUTINE>(
                GetProcAddress(
                    kernel32,
                    "LoadLibraryW"));

    if (load_library == nullptr) {
        result.error = Win32Error(
            "GetProcAddress(LoadLibraryW)",
            GetLastError());

        cleanup_memory();
        return result;
    }

    HANDLE raw_thread = CreateRemoteThread(
        process.get(),
        nullptr,
        0,
        load_library,
        remote_memory,
        0,
        nullptr);

    if (raw_thread == nullptr) {
        result.error = Win32Error(
            "CreateRemoteThread",
            GetLastError());

        cleanup_memory();
        return result;
    }

    HandleGuard thread(raw_thread);

    const DWORD wait_result =
        WaitForSingleObject(
            thread.get(),
            15000);

    if (wait_result == WAIT_TIMEOUT) {
        result.error =
            "CreateRemoteThread did not finish within 15 seconds";
        cleanup_memory();
        return result;
    }

    if (wait_result != WAIT_OBJECT_0) {
        result.error = Win32Error(
            "WaitForSingleObject",
            GetLastError());
        cleanup_memory();
        return result;
    }

    DWORD exit_code = 0;

    if (!GetExitCodeThread(
            thread.get(),
            &exit_code)) {
        result.error = Win32Error(
            "GetExitCodeThread",
            GetLastError());
        cleanup_memory();
        return result;
    }

    result.remote_exit_code =
        static_cast<std::uint32_t>(
            exit_code);

    cleanup_memory();

    if (exit_code == 0) {
        result.error =
            "LoadLibraryW returned NULL in the target process";
        return result;
    }

    result.success = true;
    return result;
}

}

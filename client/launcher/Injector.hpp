#pragma once

#include <cstdint>
#include <string>

namespace dustwire::launcher {

struct InjectionResult {
    bool success{false};
    std::uint32_t remote_exit_code{};
    std::string error;
};

class Injector {
public:
    InjectionResult Inject(
        std::uint32_t process_id,
        const std::wstring& dll_path) const;
};

}

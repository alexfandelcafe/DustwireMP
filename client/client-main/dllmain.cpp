#include <Windows.h>

#include <memory>

#include "ClientMain.hpp"

namespace {

dustwire::client::ClientMain* g_client = nullptr;

DWORD WINAPI BootstrapThread(LPVOID parameter) {
    const HMODULE module =
        static_cast<HMODULE>(parameter);

    auto* client =
        new dustwire::client::ClientMain();

    g_client = client;

    if (!client->Initialize(module)) {
        delete client;
        g_client = nullptr;
    }

    return 0;
}

}

BOOL APIENTRY DllMain(
    HMODULE module,
    DWORD reason,
    LPVOID) {

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        HANDLE thread =
            CreateThread(
                nullptr,
                0,
                &BootstrapThread,
                module,
                0,
                nullptr);

        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }

    // v0.3 keeps the injected module loaded for the lifetime
    // of the target process. No heavy shutdown work is done in DllMain.
    return TRUE;
}

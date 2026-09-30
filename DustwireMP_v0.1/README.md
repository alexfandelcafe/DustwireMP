# DustwireMP v0.1

DustwireMP is a clean-room multiplayer framework skeleton for Red Dead Redemption 1 modding.

This first version focuses on the connection path and project structure rather than game hooking:

`launcher -> client-main -> CEF UI abstraction -> native bridge -> networking abstraction -> server`

The v0.1 build is self-contained and uses a small WinSock UDP transport so the client and server can actually exchange a HELLO/WELCOME handshake without requiring third-party SDKs. The transport layer is intentionally isolated so ENet can replace it in a later milestone without changing the protocol or UI layers.

## First-version behavior

- Starts a local UDP server on port 4674.
- Starts a client connection to `127.0.0.1:4674` by default.
- Sends `HELLO` with protocol version and player name.
- Server assigns a player ID and returns `WELCOME`.
- Client reports connection state through the native UI bridge.
- Includes an HTML/CSS/JS CEF-style UI mock that mirrors the planned browser API.
- Includes a full roadmap and architecture notes for future chats.

## Build

On Windows with Visual Studio 2022 and CMake installed:

```bat
build.bat
```

Build logs are written to `logs\\build_*.log` and the batch file stops on the first failing step.

## Run

```bat
run_server.bat
run_client.bat
```

The current v0.1 client is a console client because the real CEF SDK and RDR1 injection layer are intentionally not bundled yet. The UI API and CEF bridge interfaces are already defined so those pieces can be added without changing the networking protocol.

## Important protocol note

The packet IDs and serialization in this repository are **new DustwireMP protocol definitions**. They are not claimed to reproduce the original RDRMP wire format.

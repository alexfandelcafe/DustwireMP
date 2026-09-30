# DustwireMP

DustwireMP is a clean-room multiplayer framework for building a Red Dead Redemption 1 multiplayer client/server stack.

Architecture:

`launcher -> RDR.exe -> client-main.dll -> CEF bridge / ClientNetwork / GameBridge -> ENet -> server`

CEF is the UI layer. ENet is the transport layer. The shared protocol sits above ENet and is independent of both the browser and the game engine.

## Current version: v0.3

v0.3 adds the real Windows bootstrap path:

- `DustwireMPLauncher.exe`.
- `ProcessLocator` for finding or launching `RDR.exe`.
- `GameBuild` for PE/x64 validation.
- `Injector` for loading `DustwireMPClientModule.dll` into the target process.
- `client-main.dll` bootstrap lifecycle.
- injected-process logging.
- temporary bootstrap tick source.
- Visual Studio 18 2026 / x64 build and CI.

Important: the v0.3 tick source is a temporary worker loop. It is not the final RDR1 engine tick hook.

## v0.2 networking retained

- ENet client/server transport.
- channel 0: reliable/control traffic.
- channel 1: low-latency traffic.
- HELLO/WELCOME/READY.
- player ID allocation.
- disconnect detection.
- PING/PONG latency test.
- protocol smoke test.
- ENet loopback test.

## Build

Run:

```bat
build.bat
```

The local builder uses the `Visual Studio 18 2026` generator and x64 target. CMake added this generator in version 4.2. citeturn662304search1turn662304search2

Build output:

```text
build-vs2026/bin/Debug/
  DustwireMPLauncher.exe
  DustwireMPClient.exe
  DustwireMPClientModule.dll
  config/launcher.ini
```

Use `run_launcher.bat` for the RDR1 bootstrap.

Use `run_server.bat` and `run_client.bat` for the standalone networking test pair.

The CI job uses GitHub's `windows-2025-vs2026` image and a CMake 4.4.3 download so the generator matches the local VS 18 build. citeturn326047search0turn326047search3turn662304search4

## Launcher configuration

Edit:

`build-vs2026/bin/Debug/config/launcher.ini`

Default:

```ini
game_path=RDR.exe
client_dll=DustwireMPClientModule.dll
target_process=RDR.exe
wait_for_game_ms=30000
require_x64=true
```

Relative paths are resolved from the launcher executable directory.

## Launcher logs

```text
logs/launcher.log
logs/client-main.log
logs/build_*.log
```

## Protocol

Current DustwireMP protocol:

| Opcode | Direction | Purpose |
|---|---|---|
| `0x0001` | Client -> Server | HELLO |
| `0x0002` | Server -> Client | WELCOME |
| `0x0003` | Client -> Server | READY |
| `0x0004` | Client -> Server | PING |
| `0x0005` | Server -> Client | PONG |

Wire header:

`u8 version + u16 opcode + u32 sequence`

These are new DustwireMP protocol definitions. They are not claimed to reproduce the original RDRMP wire format.

## Development route

Read `ROADMAP.md`, `ARCHITECTURE.md`, and `docs/V0.3_BOOTSTRAP.md` before modifying the client bootstrap.

Next: identify and document the exact supported RDR1 executable build, then implement a verified engine tick source and local actor discovery through `GameBridge`.
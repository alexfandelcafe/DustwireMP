# DustwireMP

DustwireMP is a clean-room multiplayer framework for building a Red Dead Redemption 1 multiplayer client/server stack.

Architecture:

`launcher -> RDR.exe -> client-main.dll -> CEF bridge / ClientNetwork / GameBridge -> ENet -> server`

CEF is the UI layer. ENet is the transport layer. The shared protocol sits above ENet and is independent of both the browser and the game engine.

## Current version: v0.3

v0.3 adds the real Windows bootstrap path:

- `DustwireMPLauncher.exe`.
- `ProcessLocator` for finding or launching `RDR.exe`.
- `GameBuild` for PE/x64 validation and SHA-256 fingerprinting.
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

~~~bat
build.bat
~~~

The local builder uses the `Visual Studio 18 2026` generator and x64 target. CMake 4.2 or newer is required for this generator.

Build output:

~~~text
build-vs2026/bin/Debug/
  DustwireMPLauncher.exe
  DustwireMPClient.exe
  DustwireMPClientModule.dll
  config/launcher.ini
~~~

Use `run_build_probe.bat "C:\\path\\to\\RDR.exe"` to inspect an RDR1 executable without launching or injecting anything. When started without an argument, the script prompts for the full path.

Use `run_launcher.bat "C:\\path\\to\\RDR.exe"` for the RDR1 bootstrap. Its launcher log is written to `logs/launcher.log` when run from the repository root.

Use `run_server.bat` and `run_client.bat` for the standalone networking test pair.

The CI job uses GitHub's Windows VS 2026 runner image and CMake 4.4.3.

## Launcher configuration

Edit:

`build-vs2026/bin/Debug/config/launcher.ini`

Default:

~~~ini
game_path=RDR.exe
client_dll=DustwireMPClientModule.dll
target_process=RDR.exe
wait_for_game_ms=30000
require_x64=true

expected_machine=0
expected_timestamp=0
expected_image_size=0
expected_file_size=0
expected_sha256=
~~~

Relative paths are resolved from the launcher executable directory.

The exact-profile fields remain empty while discovering the supported game build. Once a real `RDR.exe` is identified, the launcher can enforce the selected fingerprint.

## Exact RDR1 build fingerprint

The launcher records:

- PE machine.
- PE timestamp.
- `SizeOfImage`.
- file size.
- SHA-256 of the complete `RDR.exe`.

Run the launcher once against the real game installation, then inspect:

~~~text
logs/launcher.log
~~~

The relevant line has the form:

~~~text
PE machine=0x8664 x64=1 timestamp=... image_size=0x... file_size=... sha256=...
~~~

Copy the observed values into `config/launcher.ini` to enable a strict profile. See `docs/V0.3_BUILD_PROFILE.md` for the workflow.

A strict profile identifies the executable build; it does not by itself validate a game-tick hook.

## Launcher logs

~~~text
logs/launcher.log
logs/client-main.log
logs/build_*.log
~~~

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

## Historical RDR1 build reference

The previous FrontierMP development repository contains an observed RDR.exe fingerprint:

~~~text
file_version=1.0.42.46611
pe_timestamp=0x673783F3
image_size=0x5A5EC600
text_rva=0x1000
text_size=0x104A140
text_fnv1a64=0xB213CBF3DEE9B6BF
machine=AMD64
~~~

Treat this as a candidate profile until the current executable is checked with `run_build_probe.bat`.

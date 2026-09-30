# DustwireMP

DustwireMP is a clean-room multiplayer framework for building a Red Dead Redemption 1 multiplayer client/server stack.

The repository is intentionally layered:

`launcher -> client-main -> CEF bridge -> client networking -> ENet -> server networking -> server game`

CEF is the UI layer. ENet is the transport layer. The shared protocol sits above ENet and is independent of both the browser and the game engine.

## Current version: v0.2

The v0.2 milestone replaces the v0.1 WinSock handshake transport with ENet while preserving the DustwireMP packet format.

Implemented now:

- ENet client/server transport.
- Two transport channels:
  - channel 0: reliable/control traffic.
  - channel 1: low-latency traffic.
- HELLO/WELCOME/READY handshake.
- Server-side player ID allocation with a 32-peer default.
- ENet disconnect detection.
- Client ping measurement through PING/PONG.
- Protocol smoke test.
- CMake FetchContent dependency pinned to a known upstream ENet commit.
- Persistent roadmap in `ROADMAP.md`.

## Protocol

Current DustwireMP protocol:

| Opcode | Direction | Purpose |
|---|---|---|
| `0x0001` | Client -> Server | HELLO |
| `0x0002` | Server -> Client | WELCOME |
| `0x0003` | Client -> Server | READY |
| `0x0004` | Client -> Server | PING |
| `0x0005` | Server -> Client | PONG |

The wire format is:

`PacketHeader + opcode payload`

where the header is encoded as:

`u8 version + u16 opcode + u32 sequence`

All integer fields in the shared codec are little-endian.

These are new DustwireMP protocol definitions. They are not claimed to reproduce the original RDRMP wire format.

## Build

Windows target:

```bat
build.bat
```

The build script:

1. Creates `build/` and `logs/`.
2. Configures Visual Studio 2022 x64.
3. Lets CMake fetch the pinned ENet dependency.
4. Builds Debug.
5. Runs the protocol smoke test.
6. Stores stdout/stderr in a timestamped log.

The first configuration requires network access to fetch ENet unless the CMake dependency has already been cached locally.

## Run

Start the server:

```bat
run_server.bat
```

Then start the current console client:

```bat
run_client.bat
```

The v0.2 client is still a console executable. The CEF bridge and web UI are preparation for the real in-game browser integration planned for v0.7 of the roadmap.

## Development route

Read `ROADMAP.md` before changing architecture. Each milestone isolates one layer so we can test networking before touching RDR1 memory hooks.

Next major milestone: v0.3 RDR1 launcher/injection and a stable game tick, followed by v0.4 two-player replication.

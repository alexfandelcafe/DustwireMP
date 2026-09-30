# DustwireMP Architecture

## Layering

```text
DustwireMPLauncher
        |
        v
    RDR.exe
        |
        v
client-main.dll
        |
        +--> CEF / browser UI
        |
        +--> ClientNetwork
        |       |
        |       v
        |    Protocol
        |       |
        |       v
        |     ENet
        |       |
        +-------+----------------------+
                                        |
                                    Internet / LAN
                                        |
                                        v
                                      ENet
                                        |
                                        v
                                  ServerNetwork
                                        |
                                        v
                                    ServerGame
```

## Client bootstrap

v0.3 now has a real Windows launcher and an injected client module:

```text
DustwireMPLauncher.exe
        |
        +--> ProcessLocator
        |       +--> find RDR.exe
        |       +--> launch RDR.exe when absent
        |
        +--> GameBuild
        |       +--> PE validation
        |       +--> x64 validation
        |
        +--> Injector
                +--> OpenProcess
                +--> VirtualAllocEx
                +--> WriteProcessMemory
                +--> CreateRemoteThread(LoadLibraryW)
                                |
                                v
                    DustwireMPClientModule.dll
                                |
                                +--> DllMain
                                +--> bootstrap thread
                                +--> ClientMain
                                      +--> Logger
                                      +--> GameBridge
                                      +--> ClientNetwork
                                      +--> BootstrapTickSource
```

The injector is only a bootstrap mechanism. The module is built x64 together with the launcher.

## Important v0.3 limitation

`BootstrapTickSource` is a temporary worker loop. It is not an RDR1 frame/update hook.

No RDR1 memory reads or writes should be added to that worker. The real game tick must be implemented only after the exact target RDR1 executable build and a verified update-function signature are documented.

## Ownership rules

### `client/client-cef`

Owns browser-facing APIs and browser events.

It must not:
- open sockets;
- own ENet peers;
- modify RDR1 actors directly;
- contain server authority.

### `client/client-networking`

Owns client connection state and packet dispatch.

It can:
- call the shared transport;
- encode/decode shared protocol messages;
- expose connection state, player ID, and ping.

It must not:
- know RDR1 memory addresses;
- manipulate browser DOM;
- implement gameplay actor logic.

### `shared/net`

Contains transport implementations.

Current implementation:
- `EnetTransport`.

Legacy:
- `UdpTransport` remains in the source tree as the v0.1 reference implementation but is no longer part of the v0.3 CMake target.

### `shared/protocol`

Owns the DustwireMP wire format above ENet.

Header:

```text
u8  version
u16 opcode
u32 sequence
payload...
```

Protocol code is independent of ENet.

### `server/server-networking`

Owns:
- ENet peer lifecycle;
- connected client table;
- packet validation;
- network-level dispatch.

### `server/server-game`

Will own:
- player state;
- replication;
- world state;
- gameplay authority.

ENet types must not leak into this module.

## ENet channels

| Channel | Intended use |
|---:|---|
| 0 | Reliable/control messages |
| 1 | Low-latency messages |

Reliability is selected per packet. Channel selection is a transport concern, not part of the application packet header.

## Current handshake

```text
Client                                Server

  | -------- ENet connect -----------> |
  |                                    |
  | -------- HELLO -----------------> |
  |                                    |
  | <------- WELCOME ---------------- |
  |                                    |
  | -------- READY -----------------> |
  |                                    |
  |          gameplay                 |
```

## RDR1 integration boundary

```text
ClientNetwork
    |
    v
GameClient / PlayerManager
    |
    v
GameBridge
    |
    v
RDR1 engine / natives / memory hooks
```

Only the final bridge talks to RDR1-specific addresses and engine objects.

Before implementing a hook:
- identify the exact target RDR1 executable build;
- record its PE fingerprint;
- document the signature/address source;
- add a runtime verification check;
- fail safely when the expected code/data is absent.

## Runtime files

```text
build-vs2026/bin/Debug/
  DustwireMPLauncher.exe
  DustwireMPClient.exe
  DustwireMPClientModule.dll
  config/launcher.ini
```

Logs:

```text
logs/launcher.log
logs/client-main.log
```

## Continuation rule

For future chats, read:
- `README.md`
- `ROADMAP.md`
- `ARCHITECTURE.md`
- `docs/V0.3_BOOTSTRAP.md`
- `docs/research/RDRMP_COMPAT_NOTES.md` when compatibility questions arise.

Then inspect the current GitHub tree and commit state before changing interfaces.

Next implementation milestone: replace `BootstrapTickSource` with a verified RDR1 game tick source for the exact supported executable build, then wire local actor discovery through `GameBridge`.
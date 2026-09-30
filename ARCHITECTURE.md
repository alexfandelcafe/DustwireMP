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

## Ownership rules

### `client/client-cef`

Owns browser-facing APIs and browser events.

It must not:
- open sockets;
- own ENet peers;
- modify RDR1 actors directly;
- contain server authority.

### `client/client-networking`

Owns connection state and packet dispatch for the client.

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
- `UdpTransport` remains in the source tree as the v0.1 reference implementation but is no longer part of the v0.2 CMake target.

### `shared/protocol`

Owns the DustwireMP wire format above ENet.

Header:

```text
u8  version
u16 opcode
u32 sequence
payload...
```

The protocol is independent of ENet. Replacing ENet later must not require changing packet encoders.

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

DustwireMP currently reserves two channels:

| Channel | Intended use |
|---:|---|
| 0 | Reliable/control messages |
| 1 | Low-latency messages |

Reliability is selected per packet. Channel selection is part of the transport call, not the application protocol header.

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

HELLO contains:
- protocol version;
- client version;
- player name.

WELCOME contains:
- server-assigned player ID;
- server name.

## Current network test

The v0.2 executable pair is intended to be tested as:

1. Start `DustwireMPServer.exe`.
2. Start `DustwireMPClient.exe`.
3. Issue `connect` in the client console.
4. Server should log HELLO and a player ID.
5. Client should log WELCOME and its player ID.

Future transport smoke tests should exercise the actual client/server ENet loopback.

## RDR1 integration boundary

The future v0.3 implementation must preserve this boundary:

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

Before implementing any hook:
- identify the exact target RDR1 executable build;
- document the signature/address source;
- add a verification check;
- fail safely when the expected code/data is absent.

## Continuation rule

For future chats, read:
- `README.md`
- `ROADMAP.md`
- `ARCHITECTURE.md`

Then inspect the current GitHub tree before changing interfaces.

The next implementation milestone is v0.3:
`launcher -> RDR.exe discovery -> DLL injection -> client-main.dll -> stable game tick -> GameBridge`.

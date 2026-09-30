# RDRMP compatibility / reverse-engineering notes

This document preserves findings about the original RDRMP implementation that motivated DustwireMP.

DustwireMP is not currently implementing the original wire format. These notes are reference material for a future compatibility mode or for comparing behavior.

## Original high-level architecture observed

```text
RDRMP.exe
   |
   +--> starts / finds RDR.exe
   |
   +--> injects client-main.dll
              |
              +--> client-networking.dll
              |       |
              |       +--> ENet
              |
              +--> client-luascripting.dll
              +--> UI / CEF-related code

server-main.exe
   |
   +--> server-networking.dll
   +--> server-luascripting.dll
   +--> server-http.dll
   +--> resources/
```

## Original client packet IDs reconstructed

### Client -> Server

| Packet | Meaning | Observed payload |
|---:|---|---|
| 0 | ClientWelcome | actor model, player name, position, rotation |
| 1 | ServerEvent | event name + serialized event payload |
| 2 | PlayerTransform | position XYZ + rotation XYZ |
| 3 | Chat | chat message |

### Server -> Client

| Packet | Meaning | Observed payload |
|---:|---|---|
| 3 | ClientEvent | client-side event payload |
| 5 | TimeOfDay | time/world-time values |
| 6 | PlayerModel | uint16 player ID + uint32 model/state value |
| 7 | PlayerCreate | uint16 player ID + model + dynamic data + position + rotation |
| 8 | PlayerTransform | uint16 player ID + position + rotation |
| 9 | PlayerDelete | uint16 player ID |
| 10 | Chat | chat text |

Packet 4 was observed in the server send path near `ServerSend::ClientInfos`, but a matching active client receiver was not located in the analyzed build. Treat it as uncertain until independently verified.

## Original networking layer

The analyzed client networking DLL exposed these conceptual operations:

- `ENetClient::Connect`
- `ENetClient::Disconnect`
- `ENetClient::Send`
- `ENetClient::Poll`
- `ENetClient::RegisterPacket`

The transport wraps ENet messages with a custom message/packet layer before registered packet handlers receive them.

The exact byte-level framing of the original custom message envelope is not fully verified. Do not copy an assumed layout into DustwireMP without further reconstruction.

## Original client packet handling observations

- Packet 7 creates remote players and contains the client error string indicating duplicate player IDs are rejected.
- Packet 8 updates a remote player's position/orientation and stores transform deltas.
- Packet 9 deletes a remote player and reports an error when the player ID does not exist.
- Packet 6 updates player model/state.
- Packet 10 feeds chat/UI handling.
- Client packet 2 is emitted from the local player's game tick after reading actor/GOH transform data.

## Original server observations

The server networking DLL exported conceptual operations including:

- `ENetServer::Broadcast`
- `ENetServer::BroadcastPeerMessage`
- `ENetServer::Send`
- `ENetServer::SendPeerMessage`
- `ENetServer::Poll`
- `ENetServer::RegisterPacket`
- `GameServer::OnConnect`
- `GameServer::OnDisconnect`
- `GameServer::OnTick`
- `PlayerManager::Get`
- `PlayerManager::SetModel`
- `TimeOfDayManager::SetTime`

The analyzed server registered handlers for packet IDs 0, 1, 2, and 3.

## Important compatibility distinction

DustwireMP currently uses a new protocol:

```text
u8  protocol version
u16 opcode
u32 sequence
payload
```

with:

- channel 0 = reliable/control;
- channel 1 = low-latency traffic.

The original RDRMP implementation uses its own ENet/custom-message layer and its own packet IDs.

Future compatibility work should therefore be isolated behind a separate protocol adapter, for example:

```text
client-networking/
  DustwireProtocol
  RdrmpCompatProtocol
```

Do not rewrite the DustwireMP protocol to match the original merely to add compatibility.

## Source repositories used during analysis

- `alexfandelcafe/DLL-descomprimidos-`
- `alexfandelcafe/client-rdrmp2`
- `alexfandelcafe/RDRMP-Server-Revival-Test1`

The original binaries contained large deleted blobs in the public client repository; this document intentionally records findings rather than copying those binaries or decompiled source into DustwireMP.

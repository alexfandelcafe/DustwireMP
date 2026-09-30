# DustwireMP — Development Route

This file is the persistent project roadmap for future chats.

## Current state: v0.2 — ENet transport

Completed foundations:

- Repository structure.
- Shared protocol types.
- Binary reader/writer.
- Versioned packet header.
- CEF-facing browser API abstraction.
- HTML/CSS/JS UI mock.
- ENet client/server transport.
- Reliable control traffic on channel 0.
- Low-latency traffic on channel 1.
- HELLO/WELCOME/READY handshake.
- Server player-ID allocation.
- ENet peer disconnect detection.
- Client ping measurement.
- Protocol smoke test.
- ENet client/server loopback test.
- Reproducible ENet dependency pin.
- Windows build script with configure/build/test logging.

Current packet set:

- `0x0001 HELLO`: protocol version, client version, player name.
- `0x0002 WELCOME`: assigned player ID and server name.
- `0x0003 READY`: client acknowledges world entry.
- `0x0004 PING`: connectivity test.
- `0x0005 PONG`: connectivity response.

Transport rules:

- Channel 0 is used for reliable/control messages.
- Channel 1 is used for low-latency messages.
- Gameplay code does not include ENet headers.
- The shared packet format stays above the transport.
- `EnetTransport` owns ENet peer/host lifecycle.
- `ClientNetwork` owns protocol-level connection state.

The exact original RDRMP wire layout is a separate reverse-engineering task and is not part of the DustwireMP protocol.

## v0.3 — RDR1 client launcher / injection / game tick

Goal: get `client-main.dll` loaded by the target RDR1 build and execute a stable client tick.

Work items:

- `DustwireMPLauncher.exe` process discovery.
- Targeted RDR.exe version detection.
- DLL injection into the selected process.
- Injected-process logger.
- Stable initialization/shutdown lifecycle.
- Game tick hook.
- Local actor discovery.
- First GameBridge implementation.
- Configuration for the RDR1 executable path.

Suggested initial client project split:

```text
client/
  launcher/
    LauncherMain.cpp
    ProcessLocator.cpp
    Injector.cpp
    GameBuild.cpp
  client-main/
    ClientMain.cpp
    ClientMain.hpp
  client-game/
    GameBridge.hpp
    Rdr1GameBridge.cpp
    GameTick.hpp
  client-networking/
    ClientNetwork.hpp/.cpp
```

Suggested client flow:

`DustwireMPLauncher -> RDR.exe -> client-main.dll -> ClientMain::Initialize -> GameBridge -> ClientNetwork -> Tick`

The first v0.3 target should be a successful DLL load + one stable tick, not multiplayer replication.

Do not hard-code addresses from an unrelated game build. Each memory signature/address must be verified against the targeted executable before shipping it.

## v0.4 — Two-player replication

Goal: two clients can see each other.

Server authority:

- Assign player IDs.
- Track connection state.
- Track model, position, rotation.
- Broadcast spawn/despawn.
- Broadcast transforms.
- Reject malformed or oversized packets.
- Maintain a server-side player state table.

Client:

- Local player transform capture.
- Remote actor creation.
- Remote actor deletion.
- Transform interpolation.
- Basic range/interest filtering.

Suggested packet IDs:

- `0x0100 PLAYER_SPAWN`
- `0x0101 PLAYER_DELETE`
- `0x0102 PLAYER_TRANSFORM`
- `0x0103 PLAYER_MODEL`

Suggested update rate:

- Simulation/network input: 20–30 Hz initially.
- Remote visual interpolation: render frame rate.
- Final rates should be measured rather than assumed.

## v0.5 — Chat and events

- Client -> server chat.
- Server -> client chat.
- Client events.
- Server events.
- Event payload serializer.
- Rate limiting.
- Message length limits.

## v0.6 — Lua resources

Resource structure:

```text
resources/
  chat/
    manifest.toml
    client.lua
    server.lua
```

Lua APIs to target:

- `RegisterNetEvent`
- `TriggerServerEvent`
- `TriggerClientEvent`
- `AddEventHandler`

Keep scripts independent from the transport implementation.

## v0.7 — Real CEF integration

Goal: replace the UI mock with the actual CEF SDK inside the client.

Planned browser API:

```text
rdrmp.connectToServer(host, port)
rdrmp.disconnect()
rdrmp.getConnectionState()
rdrmp.getPing()
rdrmp.on(eventName, callback)
```

Native -> browser events:

```text
connecting
connected
handshaking
loadingWorld
disconnected
connectionError
serverInfo
pingUpdated
```

CEF must not own sockets, ENet peers, gameplay state, or RDR1 actor state.

## v0.8 — Server browser / master server

Architecture:

`CEF -> HTTPS API -> master server`

Gameplay remains:

`client -> ENet -> game server`

Master server responsibilities:

- Server registration.
- Heartbeat.
- Player count.
- Server metadata.
- Optional authentication later.

## v0.9 — World synchronization

- Time of day.
- Weather.
- Spawn points.
- Interior/world state.
- Basic streaming/range checks.
- Initial authoritative world state.

## v1.0 — Production foundation

- Crash-safe logging.
- Version negotiation.
- Resource download/cache.
- Configuration system.
- Admin permissions.
- Security/rate limits.
- Performance profiling.
- Automated protocol/transport tests.
- Dedicated server packaging.
- Release packaging for client and server.

## Persistent next-chat checklist

When continuing the project, use this order:

1. Read `README.md`, `ROADMAP.md`, and `ARCHITECTURE.md`.
2. Inspect the current GitHub tree and commit state before changing interfaces.
3. Keep protocol definitions in `shared/protocol/`.
4. Keep transport implementations in `shared/net/`.
5. Do not reintroduce direct ENet calls into gameplay classes.
6. For RDR1 integration, verify the exact game executable/build before using signatures or memory addresses.
7. Update the roadmap after completing a milestone.

## Next chat starting point

Implement **v0.3**:

`launcher -> RDR.exe discovery -> version check -> DLL injection -> client-main.dll -> stable game tick -> GameBridge stub`

Then connect `ClientNetwork::Tick()` to that game tick without moving ENet code into GameBridge.

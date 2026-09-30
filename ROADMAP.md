# DustwireMP — Development Route

This file is the persistent project roadmap for future chats.

## Current state: v0.1 — Connection Skeleton

Completed foundations:

- Repository structure.
- Shared protocol types.
- Binary reader/writer.
- Versioned packet header.
- WinSock UDP transport abstraction.
- HELLO/WELCOME handshake.
- Server player-ID allocation.
- Client connection state machine.
- CEF-facing browser API abstraction.
- HTML/CSS/JS UI mock.
- Prolix Windows build script with error log.

Current packet set:

- `0x0001 HELLO`: protocol version, client version, player name.
- `0x0002 WELCOME`: assigned player ID and server name.
- `0x0003 READY`: client acknowledges world entry.
- `0x0004 PING`: connectivity test.
- `0x0005 PONG`: connectivity response.

## v0.2 — Real ENet transport

Goal: replace only `UdpTransport` with ENet.

Work items:

- Add ENet as `third_party` or CMake dependency.
- Implement reliable/unreliable channels.
- Preserve `PacketHeader` + payload protocol.
- Add connection timeout and peer disconnect handling.
- Add ping measurement.

Do not mix ENet concerns into gameplay classes.

## v0.3 — RDR1 client injection / game bridge

Goal: get `client-main.dll` loaded by RDR1 and execute a stable game tick.

Work items:

- Launcher process discovery.
- DLL injection.
- `GameBridge` interface.
- Game tick hook.
- Local actor discovery.
- Safe logging inside injected process.

The exact RDR1 addresses/signatures must be verified against the targeted game build before implementing memory hooks.

## v0.4 — Player replication

Goal: two clients can see each other.

Server authority:

- Assign player IDs.
- Track connection state.
- Track model, position, rotation.
- Broadcast spawn/despawn.
- Broadcast transforms.

Client:

- Local player transform capture.
- Remote actor creation.
- Remote actor deletion.
- Transform interpolation.

Suggested packet IDs:

- `0x0100 PLAYER_SPAWN`
- `0x0101 PLAYER_DELETE`
- `0x0102 PLAYER_TRANSFORM`
- `0x0103 PLAYER_MODEL`

## v0.5 — Chat and events

- Client -> server chat.
- Server -> client chat.
- Client events.
- Server events.
- Event payload serializer.
- Rate limiting.

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

The exact API can evolve; keep resource scripts independent from the network transport.

## v0.7 — CEF integration

Goal: replace UI mock with real CEF.

Planned browser API:

```text
rdrmp.connectToServer(host, port)
rdrmp.disconnect()
rdrmp.getConnectionState()
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
```

CEF should never own sockets or game state.

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
- Optional favorites/auth in a later phase.

## v0.9 — World synchronization

- Time of day.
- Weather.
- Interior/world state.
- Spawn points.
- Basic streaming/range checks.

## v1.0 — Production foundation

- Crash-safe logging.
- Version negotiation.
- Resource download/cache.
- Configuration system.
- Admin permissions.
- Security/rate limits.
- Performance profiling.
- Automated protocol tests.
- Dedicated server packaging.

## Next chat starting point

Start from `v0.1` and implement **v0.2 ENet** without changing the shared packet format. Then move to **v0.3 RDR1 injection + GameBridge**.

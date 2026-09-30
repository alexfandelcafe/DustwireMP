# DustwireMP — Development Route

This file is the persistent project roadmap for future chats.

## Current state: v0.3 — launcher and injected client bootstrap

Completed:

- v0.1 repository and protocol skeleton.
- v0.2 ENet transport.
- v0.2 reliable channel 0 / low-latency channel 1 split.
- v0.2 HELLO/WELCOME/READY handshake.
- v0.2 ping/disconnect handling.
- v0.2 protocol and ENet loopback tests.
- Visual Studio 18 2026 x64 local builder.
- Windows CI using the VS 2026 runner image.
- v0.3 DustwireMPLauncher.exe.
- v0.3 ProcessLocator.
- v0.3 PE/x64 GameBuild validation.
- v0.3 PE fingerprint logging.
- v0.3 SHA-256 executable fingerprinting.
- v0.3 Injector.
- v0.3 DustwireMPClientModule.dll.
- v0.3 injected logging.
- v0.3 bootstrap tick source.

## v0.3 remaining — verified RDR1 game tick

Do not mark this complete until the exact target RDR1 executable build is identified.

Tasks:

- Capture the current fingerprint from the real target RDR.exe (completed; local probe matched the historical candidate).
- Decide the supported executable build/profile (completed: RDR1 1.0.42.46611).
- Add the exact fingerprint to a strict launcher profile (completed; full PE/text/file/SHA-256 profile is enforced).
- Resolve the game-thread entry using the historical `scrThread::Wait` dispatcher.
- Add `Rdr1GameTickSource` and wire it into `ClientMain`.
- Keep the hook failure-safe.
- Begin local actor discovery through `GameBridge`.

## Historical reference discovered

The previous FrontierMP branch contains a tested build candidate for RDR.exe 1.0.42.46611:

- PE timestamp 0x673783F3
- SizeOfImage 0x5A5EC600
- .text RVA 0x1000
- .text size 0x104A140
- .text FNV-1a64 0xB213CBF3DEE9B6BF
- AMD64

It also contains a GameThreadDispatcher built around the scrThread::Wait native context. This is reference material for DustwireMP, not a claim that the current installation or hook is already validated.

## v0.4 — Two-player replication

Goal: two clients can see each other.

Server:

- authoritative player table.
- player IDs.
- model.
- position.
- rotation.
- spawn/despawn broadcast.
- transform broadcast.
- malformed-packet limits.

Client:

- local actor transform capture.
- remote actor creation.
- remote actor deletion.
- transform interpolation.
- basic interest/range filtering.

Suggested packet IDs:

- 0x0100 PLAYER_SPAWN
- 0x0101 PLAYER_DELETE
- 0x0102 PLAYER_TRANSFORM
- 0x0103 PLAYER_MODEL

## v0.5 — Chat and events

- Client -> server chat.
- Server -> client chat.
- Client events.
- Server events.
- event serializer.
- rate limits.
- message limits.

## v0.6 — Lua resources

~~~text
resources/
  chat/
    manifest.toml
    client.lua
    server.lua
~~~

Target APIs:

- RegisterNetEvent
- TriggerServerEvent
- TriggerClientEvent
- AddEventHandler

## v0.7 — Real CEF integration

Goal: replace the UI mock with actual CEF inside the client.

Browser API:

~~~text
rdrmp.connectToServer(host, port)
rdrmp.disconnect()
rdrmp.getConnectionState()
rdrmp.getPing()
rdrmp.on(eventName, callback)
~~~

CEF must not own sockets, ENet peers, gameplay state, or RDR1 actor state.

## v0.8 — Server browser / master server

~~~text
CEF -> HTTPS API -> master server
~~~

Gameplay remains:

~~~text
client -> ENet -> game server
~~~

## v0.9 — World synchronization

- Time of day.
- Weather.
- Spawn points.
- Interior/world state.
- Streaming/range checks.

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
- Release packaging.

## Persistent next-chat checklist

1. Read README.md, ROADMAP.md, ARCHITECTURE.md, and docs/V0.3_BOOTSTRAP.md.
2. Inspect the current GitHub tree and latest commit.
3. Keep protocol code in shared/protocol/.
4. Keep transport code in shared/net/.
5. Keep RDR1-specific code behind GameBridge / game-tick interfaces.
6. Never ship an RDR1 memory address without verifying it against the supported executable build.
7. Update this document after each milestone.

## Next chat starting point

Implement the verified RDR1 game tick:

RDR.exe build fingerprint -> signature/profile -> Rdr1GameTickSource -> ClientMain::Tick -> GameBridge

Then use the verified tick to discover the local actor before implementing v0.4 replication.

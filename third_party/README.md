# Third-party dependencies

## ENet

DustwireMP uses the upstream ENet reliable-UDP library:

https://github.com/lsalzman/enet

The build fetches a pinned upstream commit:

`5a9c537fd464b3c6d3c55e1d3bd47588faf71b42`

ENet is linked only by `dustwire_shared`. Gameplay, UI, and the DustwireMP packet protocol do not call ENet APIs directly.

The upstream project is MIT licensed. See the upstream repository for its license text.

## CEF

CEF is not bundled yet. The repository currently contains the native browser bridge and a UI mock. The real CEF SDK will be introduced during the RDR1 client integration milestone, keeping CEF binaries outside the gameplay/network protocol code.

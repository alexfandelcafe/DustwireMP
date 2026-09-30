# Third-party dependencies

The v0.1 source tree intentionally has no bundled CEF or ENet SDK.

Next milestone:

- Add CEF SDK here for the in-process/injected browser UI.
- Add ENet here (or as a package/CMake dependency) and implement `ITransport` with an `EnetTransport`.

Keep third-party sources separate from project code.

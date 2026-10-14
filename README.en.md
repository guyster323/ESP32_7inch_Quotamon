# Quotamon

**AI subscription quotas on one 7-inch desk display.**

[한국어](README.md) · [Architecture](docs/ARCHITECTURE.md) · [Orca integration](docs/ORCA_INTEGRATION.md)

![Native LVGL mock dashboard](visuals/native/09-five-providers.png)

This is a real LVGL/RGB565 render with illustrative data, not a photograph of a flashed device.

Quotamon targets the **Waveshare ESP32-S3-Touch-LCD-7 Rev1.2**, with five account cards per page, touch details, multiple quota pools, configurable fields, Korean/Latin fonts, and six pixel-art states for Codex, Cursor, Copilot, Claude, and Grok. The percentage text changes from white to dark across the fill boundary, inspired by [F108pro Usage Monitor](https://github.com/guyster323/F108pro_Usage_monitor).

The product architecture is **Orca-integrated BLE central → standalone ESP32 BLE peripheral**. No separate PC companion or tray collector is included. Authentication stays with Orca and the providers; only sanitized quota fields cross BLE. ESP32 renders independently, but a running Orca PC is still required to supply subscription data. This does not offer PC-free cloud sign-in.

## Development preview

Stock Orca does **not** contain the BLE sender. This repository provides a source integration patch, main-process modules, settings/onboarding pane and a bundled Windows BLE resource. The patch was checked against Orca commit `3f81541188b435aabc822aaf36c6f23a70e1a783` (1.4.214). A complete patched Orca build and physical BLE pairing are not yet validated.

Native UI/pointer tests, C++/TypeScript fragmentation and privacy tests pass. The Windows resource compiles and detects BLE central support on the development PC. Firmware builds with ESP-IDF 5.5.3 and fits the 8 MB factory partition layout (about 6.36 MB). See [verification](docs/VERIFICATION.md) for precise boundaries.

Codex and Claude support Orca-managed accounts. Cursor and Grok currently expose one active account through the reviewed Orca API. Copilot uses an optional read-only quota RPC from an already installed Windows CLI; its SDK/AIC mapping is experimental. Missing data remains unknown; session usage is never treated as monthly quota. No universal 6,500-credit entitlement is assumed.

Default snapshot interval: 60 seconds. Heartbeat: 10 seconds. Values clear after disconnect or 35 seconds without traffic. Source freshness remains independent of BLE liveness. Pairing uses encrypted Secure Connections with numeric comparison and approval on both screens.

## Build

```powershell
python tools/bootstrap_dependencies.py
npm ci --prefix orca-integration
npm test --prefix orca-integration
tools\build_native.cmd
build\native\quotamon_protocol_test.exe
build\native\espmonitor_sim.exe
```

The simulator is a development tool. Follow [Orca integration](docs/ORCA_INTEGRATION.md) and [firmware upload](docs/UPLOAD.md) for hardware setup. USB is used for flashing/power/logs; Wi-Fi and classic Bluetooth SPP are not used.

MIT project code. F108pro artwork/glyph shapes, Noto fonts, Orca patch context, Waveshare port and runtime dependencies retain their respective [notices](THIRD_PARTY_NOTICES.md). The new lavender Copilot pilot robot is original, unofficial artwork.

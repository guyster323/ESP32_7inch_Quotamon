# Orca + standalone display

The ESP32 owns the LVGL UI, touch input, BLE server, framing validation, bonding and offline state. Unmodified Orca owns provider login and rate-limit fetching/caching. A portable Windows bridge reads its existing CLI, owns display account selection/settings and runs the BLE central. The tray launcher bundles Node and .NET; users extract a ZIP rather than install runtimes or rebuild Orca. BLE runs as a private mode of the same Windows executable, communicating with the controller over stdin/stdout.

## Lifecycle

1. The portable tray launcher starts the bridge. If a previously approved device address is saved and Orca is available, the controller reconnects using the existing Windows bond.
2. A first connection requires the device's two-minute pairing window and six-digit comparison on both screens. Windows pairing requires encryption and authentication; ESP32 disables legacy pairing and requires Secure Connections/MITM protection.
3. The bridge reads `orca account list --json` every 15 seconds. Provider-cache refresh uses the existing owner-local `accounts.list` IPC at most every 15 minutes. The local runtime token stays in memory and never crosses BLE. It builds a new allowlisted object rather than serializing raw account records.
4. Snapshots carry ordered account selections, independent quota pools, source timestamps, reset descriptions and display preferences. ESP32 applies only a complete, CRC-valid, schema-valid live snapshot, then acknowledges it.
5. Touch refresh/settings requests travel back through the control characteristic. Settings use a revision to reject conflicting PC/device changes.
6. Orca loss clears bridge account data and disconnects BLE on the next cache check. Link loss or 35 seconds without valid traffic clears device values. Reconnection needs a new valid snapshot. Closing the tray launcher terminates its Node/BLE/SDK children; a Windows kill-on-close job also handles launcher crashes.

## Scheduling

| Operation | Default |
|---|---|
| Snapshot transmission | 60 s, configurable 15–3600 s |
| Heartbeat | 10 s |
| Read running Orca cache / detect loss | 15 s |
| Link freshness limit | 35 s |
| Source stale-aware refresh request | 15 min |
| Manual refresh coalescing/throttle | 30 s minimum; Orca's source rules still apply |
| Old source marker | Older than 30 min, fetch error, or elapsed reset window |
| Account/pool bounds | 20 accounts, 8 pools per account |
| Wire message bound | 32 KiB including CRC |

Transport freshness and provider freshness are different. A fresh heartbeat cannot turn an old provider cache into a fresh quota. Quota windows remain separate; they are not averaged into a fabricated overall percentage.

## Standalone boundary

The board needs power and BLE, and does not need a Wi-Fi network. Orca and the portable bridge must run on a nearby Windows PC. Stock Orca does not send BLE itself; the bridge provides that operation without modifying Orca. Reading Orca's private LAN WebSocket from an independent ESP32 would require its authenticated paired-device/E2EE protocol and network access; it is not a public quota HTTP endpoint. The board does not sign into cloud accounts independently.

## Data boundary

Allowed fields are constructed in `orca-integration/src/quota-snapshot.ts`. IDs are provider-prefixed hashes; default aliases reject email addresses. Account auth, email, refresh/access tokens, Orca runtime tokens, conversation text, file paths and payment data never enter the display snapshot. Windows Bluetooth addresses stay in the bridge's local configuration, not in the quota payload or public test artifacts.

Unselected accounts contribute only the sanitized account directory needed by the touchscreen selector. Their quota values, source timestamps and amount/session fields are removed before BLE serialization. Cached snapshot changes and source refreshes trigger normalization; the bridge does not rediscover provider authentication files on UI polls. Copilot optionally uses its already installed SDK quota RPC with no model session.

The settings UI binds only to 127.0.0.1 on a random port. API calls require a fresh process-local key and exact Host/Origin validation, have no CORS access, and are bounded. Configuration contains selections, aliases, display preferences and the device address. Provider credentials are not persisted. Startup uses the current user's Windows Run key only when the user saves that preference.

The older Orca main-process patch remains an optional developer integration, not a requirement for the portable product. Its full application build remains unverified.

Firmware pairing bonds are stored through NimBLE/NVS. The screen can explicitly clear a bond for another PC; the previous Windows Bluetooth device must also be removed before re-pairing. Offline display state contains no cached numeric quota.

## Hardware choices

- ESP32-S3 uses **BLE**, not classic Bluetooth/SPP.
- The official LCD-7 RGB port initializes CH422G, RGB framebuffer and GT911. LVGL buffers/fonts use PSRAM.
- Brightness is software dimming over the rendered UI. The vendor CH422G backlight output is an on/off signal; this setting is not hardware PWM or a power-saving backlight controller.
- The initial firmware uses an 8 MB factory-app partition layout, with no OTA slot. Verify actual flash capacity and linked image size before upload.

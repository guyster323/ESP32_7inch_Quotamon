# Orca + standalone display

The ESP32 owns the LVGL UI, touch input, BLE server, framing validation, bonding and offline state. Orca owns login credentials, account selection, rate-limit fetch/cache, the BLE central lifecycle and persistent display configuration. There is no separate companion process that collects or authenticates provider data. The small native Windows resource is bundled with, started by and terminated with Orca; it handles only Bluetooth operations over private stdin/stdout.

## Lifecycle

1. Orca starts. If a previously approved device address is saved, its integrated controller reconnects using the existing Windows bond.
2. A first connection requires the device's two-minute pairing window and six-digit comparison on both screens. Windows pairing requires encryption and authentication; ESP32 disables legacy pairing and requires Secure Connections/MITM protection.
3. The controller reads Orca's internal account snapshot and change events. It builds a new allowlisted object rather than serializing raw account records.
4. Snapshots carry ordered account selections, independent quota pools, source timestamps, reset descriptions and display preferences. ESP32 applies only a complete, CRC-valid, schema-valid live snapshot, then acknowledges it.
5. Touch refresh/settings requests travel back through the control characteristic. Settings use a revision to reject conflicting PC/device changes.
6. Closing Orca ends its resource. Link loss or 35 seconds without valid traffic clears numeric data. Reconnection needs a new valid snapshot; it never revives values retained from an old connection.

## Scheduling

| Operation | Default |
|---|---|
| Snapshot transmission | 60 s, configurable 15–3600 s |
| Heartbeat | 10 s |
| Link freshness limit | 35 s |
| Source stale-aware refresh request | 15 min |
| Manual refresh coalescing/throttle | 30 s minimum; Orca's source rules still apply |
| Old source marker | Older than 30 min, fetch error, or elapsed reset window |
| Account/pool bounds | 20 accounts, 8 pools per account |
| Wire message bound | 32 KiB including CRC |

Transport freshness and provider freshness are different. A fresh heartbeat cannot turn an old provider cache into a fresh quota. Quota windows remain separate; they are not averaged into a fabricated overall percentage.

## Standalone boundary

The board needs power and BLE, and does not need a Wi-Fi network. Orca must run on a nearby Windows PC. Reading Orca's private LAN WebSocket from an independent ESP32 would require its authenticated paired-device/E2EE protocol and network access; it is not a public quota HTTP endpoint. That route and a separately installed PC collector are outside this product configuration.

## Data boundary

Allowed fields are constructed in `orca-integration/src/quota-snapshot.ts`. IDs are provider-prefixed hashes; default aliases reject email addresses. Account auth, email, refresh/access tokens, Orca runtime tokens, conversation text, file paths and payment data never enter the display snapshot. Windows Bluetooth addresses stay in Orca's local configuration, not in the quota payload or public test artifacts.

Unselected accounts contribute only the sanitized account directory needed by the touchscreen selector. Their quota values, source timestamps and amount/session fields are removed before BLE serialization. Orca's account snapshot is normalized on change events and source refreshes, rather than rediscovering PC authentication files on every UI poll.

Firmware pairing bonds are stored through NimBLE/NVS. The screen can explicitly clear a bond for another PC; the previous Windows Bluetooth device must also be removed before re-pairing. Offline display state contains no cached numeric quota.

## Hardware choices

- ESP32-S3 uses **BLE**, not classic Bluetooth/SPP.
- The official LCD-7 RGB port initializes CH422G, RGB framebuffer and GT911. LVGL buffers/fonts use PSRAM.
- Brightness is software dimming over the rendered UI. The vendor CH422G backlight output is an on/off signal; this setting is not hardware PWM or a power-saving backlight controller.
- The initial firmware uses an 8 MB factory-app partition layout, with no OTA slot. Verify actual flash capacity and linked image size before upload.

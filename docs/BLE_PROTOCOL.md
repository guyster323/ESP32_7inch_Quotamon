# Quotamon BLE protocol v1

ESP32: peripheral/GATT server. Orca: central/GATT client. Service UUID `7155e001-9df0-4b75-9d9b-796d6f6e6974`.

| Characteristic | UUID ending | Direction |
|---|---|---|
| Snapshot/heartbeat | `...e002-...` | Orca write-with-response; encrypted/authenticated |
| Control | `...e003-...` | ESP32 notifications on authenticated link |

Full characteristic UUIDs are defined in `orca-integration/src/ble-protocol.ts` and `firmware/main/ble_transport.cpp`. Notifications do not carry raw account credentials. CRC is an integrity/fragment check; BLE encryption and authenticated pairing provide link security.

## Framing

Every characteristic value contains an eight-byte header and at least one payload byte. All multibyte integers are little-endian.

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 1 | High nibble version 1, low nibble kind: snapshot=1, control=2, heartbeat=3 |
| 1 | 1 | Flags: bit0 START, bit1 END; other bits forbidden |
| 2 | 2 | Sequence, modulo 65536 |
| 4 | 2 | Byte offset into complete message |
| 6 | 2 | Total message length, including four CRC bytes |
| 8 | variable | Message slice |

Append CRC32/IEEE (`0xedb88320`, initial/final XOR `0xffffffff`) as four little-endian bytes to the body before fragmentation. Maximum total length 32768. START must have offset zero. All fragments must retain kind/sequence/total and arrive contiguously. Gaps, duplicates, overflows, invalid flags and CRC failures discard the message. A new START replaces an unfinished message.

At ATT MTU 23 the characteristic value is 20 bytes: 8 header + 12 message bytes. Negotiate a larger MTU when available; never infer it from the subscription payload size. Both implementations also test value sizes 244 and 512. C++ and TypeScript share this golden frame for snapshot body `abc`, sequence `0x1234`:

```text
11 03 34 12 00 00 07 00 61 62 63 c2 41 24 35
```

## Message bodies

- Snapshot: UTF-8 JSON with `protocol: "quotamon.v1"`, `produced_at` Unix seconds, `refreshed` (the PC-local snapshot creation time for the LCD footer), `config_revision`, `connected:true`, `mock:false`, `accounts`, `display`. The C++ decoder enforces provider, uniqueness, number, label, account and pool bounds. Older snapshots without `refreshed` show an unknown refresh time; provider collection timestamps remain independent.
- Heartbeat: exactly eight bytes, Unix milliseconds as uint64 LE, then the framing CRC. It does not refresh source quota timestamps.
- Control: bounded UTF-8 JSON. Allowed requests: `refresh`; `settings` with `base_revision`, display fields and `{id,enabled}` selections; `ack` with completed snapshot sequence.
- Optional diagnostics: `runtime_stats` with `schema:1` is a device-to-PC control notification every five seconds when `CONFIG_QUOTAMON_RUNTIME_METRICS` is enabled. It contains CPU/runtime counters, completed LVGL refresh counts/times, numeric page/visible-character counts and heap statistics. It contains no accounts, credentials or addresses. Older clients ignore this notification. It uses the existing authenticated connection and does not change the snapshot/heartbeat protocol.

Install the ACK waiter before sending: a notification may arrive before the final GATT write completes. Snapshots are serialized/coalesced; one outstanding snapshot is allowed. ACK deadline is 10 s after writes, transfer deadline 30 s. Firmware contact time advances during valid fragments, so a fragmented transfer does not wait until completion to count as link activity. Failed or stale-revision settings requests do not silently change Orca configuration.

Connection epochs prevent queued data/control from a previous link leaking into a new one. LVGL is touched only on the UI thread, outside NimBLE callbacks. New pairing is allowed only after the user opens a window on the touchscreen; automatic numeric comparison acceptance is prohibited.

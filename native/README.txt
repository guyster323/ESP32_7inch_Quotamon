Quotamon native LVGL 8.4 UI

native/ui.cpp is shared by the Windows simulator and ESP-IDF board firmware.
Five account cards per page, up to 20 accounts, 8 pools/account, five provider sprites with six states, detail/settings/account selection and BLE numeric comparison screens.

Build: python tools/bootstrap_dependencies.py; tools/build_native.cmd
Mock preview: build/native/espmonitor_sim.exe
Hidden renderer checks: python tools/verify_native_visuals.py
Protocol checks: build/native/quotamon_protocol_test.exe

The --snapshot file option is a developer fixture bridge only. Production transport is Orca-integrated BLE, not a separate PC collector.
Generated fonts/assets are committed; full Hangul + ASCII/Latin coverage requires LV_FONT_FMT_TXT_LARGE=1 consistently in LVGL and application sources. Board configuration uses malloc/PSRAM rather than the simulator's static heap.

See docs/UI_BRIEF.md, docs/ORCA_INTEGRATION.md, docs/BLE_PROTOCOL.md and docs/UPLOAD.md. Firmware/hardware validation status: docs/VERIFICATION.md.

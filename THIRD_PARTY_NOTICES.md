# Third-party notices

Codex, Cursor, Claude and Grok sprites, numeric pixel glyph shapes and split-meter design originate from [F108pro Usage Monitor](https://github.com/guyster323/F108pro_Usage_monitor), commit ddab71ab58e353a0b9d1a60274ae9a1a05b0d965. Copyright © 2026 QuotaDeck contributors. MIT license including upstream compiler attribution is preserved in [assets/providers/F108pro-LICENSE.txt](assets/providers/F108pro-LICENSE.txt) and [native/generated/F108pro-LICENSE.txt](native/generated/F108pro-LICENSE.txt).

The lavender Copilot pilot robot is original unofficial artwork. Source master, exact prompts and conversion record: [assets/copilot](assets/copilot). No official vendor mascot or logo is claimed.

Noto Sans KR Medium: SIL OFL 1.1, [full notice](native/generated/NotoSansKR-OFL.txt). Embedded glyphs include modern Hangul, ASCII and Latin extended; other scripts require additional coverage.

| Component | Version / revision | License / notice |
|---|---|---|
| LVGL | 8.4.0, 4495f428630cc1741bd8bfd977f080e8460e8e8d | MIT; retained by dependency bootstrap |
| nlohmann/json | 3.12.0 | MIT; retained by dependency bootstrap |
| ESP-IDF / NimBLE | ESP-IDF 5.5 series via PlatformIO 6.13.0 | Apache-2.0 and upstream component notices |
| Waveshare RGB port | bc51a84b30c0cea018fee6e0d7427ec10102060e | Source retains CC0-1.0 SPDX; repository Apache text in firmware/vendor/WAVESHARE-LICENSE |
| Orca patch context | 3f81541188b435aabc822aaf36c6f23a70e1a783 | MIT, © 2026 Lovecast Inc.; orca-integration/ORCA-LICENSE |
| .NET 10 / Windows SDK projection | Build-selected SDK packages | Upstream runtime/package notices; retain in distributions |
| Bundled Node | Build-selected installed runtime (22+) | Node MIT and included third-party notices; exact version/license bundled with portable ZIP |
| esbuild | 0.25.11 | MIT; build tool only, not embedded as a runtime dependency |
| Optional Copilot SDK / CLI | User's existing installation | Upstream GitHub licensing; not redistributed here |
| Pillow / fonttools / lv_font_conv | Asset build and verification tools | Upstream licenses; not embedded runtime code |

The portable bridge uses Orca's existing CLI/local account snapshot contract without modifying its application. The optional internal patch reuses Orca UI/process primitives. No provider authentication readers are copied. Generated files retain attribution and portable packages include Node/.NET/F108 notices. Older Python collector experiments remain outside the published product source.

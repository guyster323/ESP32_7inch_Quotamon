# 자료 검토와 채택 결과

기준: 2026-10-14. GitHub ESP 검색은 후보 발굴에 사용하고, 실제 보드·LVGL·전송·쿼터 구현에 맞는 공식 소스와 관련 저장소를 선별했습니다. 스타 수를 하드웨어 호환성의 증거로 사용하지 않습니다.

| 저장소 | 채택 내용 |
|---|---|
| [waveshareteam/ESP32-S3-Touch-LCD-7](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7) | 정확한 LCD-7 RGB/GT911/CH422G 포트·핀·타이밍 |
| [espressif/esp-idf](https://github.com/espressif/esp-idf) | NimBLE, PSRAM, RGB LCD, FreeRTOS |
| [lvgl/lvgl](https://github.com/lvgl/lvgl) | LVGL 8.4.0 공유 UI와 실제 렌더 검사 |
| [F108pro Usage Monitor](https://github.com/guyster323/F108pro_Usage_monitor) | 4개 픽셀 캐릭터·숫자 글리프·막대 경계의 색 분리 |
| [stablyai/orca](https://github.com/stablyai/orca) | 기존 계정/사용량 캐시·변경 이벤트·UI/프로세스 정책 |
| [nlohmann/json](https://github.com/nlohmann/json) | 스냅샷 스키마·숫자·배열 검증 |
| [github/copilot-sdk](https://github.com/github/copilot-sdk) | 기존 CLI의 읽기 전용 계정 quota RPC |
| [Noto Sans KR](https://github.com/google/fonts/tree/main/ofl/notosanskr) | OFL 한글·Latin 폰트 |

Waveshare `bc51a84b30c0cea018fee6e0d7427ec10102060e`, F108pro `ddab71ab58e353a0b9d1a60274ae9a1a05b0d965`, Orca `3f81541188b435aabc822aaf36c6f23a70e1a783`, LVGL `4495f428630cc1741bd8bfd977f080e8460e8e8d`를 검토했습니다.

## 비교한 다른 경로

[arduino-esp32](https://github.com/espressif/arduino-esp32), [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel), [ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander), [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)는 유효한 Arduino 후보입니다. 이번 구현은 공식 ESP-IDF LCD-7 포트와 내장 NimBLE로 통일합니다.

[ESPHome](https://github.com/esphome/esphome), [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)는 Wi-Fi 중심 후보, [bleak](https://github.com/hbldh/bleak)은 별도 Python BLE 앱 후보입니다. Orca+BLE 전용 방향에 따라 채택하지 않습니다. [esp-bsp](https://github.com/espressif/esp-bsp)는 공식 BSP 참고이며 다른 보드의 핀맵을 그대로 적용하지 않습니다.

## Orca를 그대로 얻어 쓰는 범위

[공식 usage tracking 문서](https://www.onorca.dev/docs/agents/usage-tracking)와 소스의 `runtime-account-controller.ts`를 검토했습니다. account snapshot, 변경 구독, stale-aware refresh가 있지만 쿼터 BLE 출력은 없습니다. **Orca를 수정하지 않고 CLI/로컬 캐시를 읽고, 포터블 중계가 BLE를 송신**합니다. 개발 PC의 설치본 1.4.224에서 실제 Codex 3계정/Cursor 읽기와 로컬 갱신 요청을 확인했습니다. 초기 내부 통합 패치는 선택적인 개발 자료로 유지합니다.

LAN 경로는 단순 HTTP JSON이 아니라 runtime RPC, paired-device 권한과 E2EE를 사용합니다. 보드에 Orca owner token을 복사하지 않습니다. 기존 background polling은 창 상태에 영향을 받으므로 중계가 15분 간격으로 PC 내부의 owner-local cache refresh를 요청합니다. 로컬 token은 해당 요청의 메모리에서만 쓰고 BLE/설정/UI에 내보내지 않습니다. 실제 provider 조회는 Orca의 캐시·Retry-After 정책이 결정합니다.

현재 소스가 관리 다중 계정으로 내보내는 대상은 Codex/Claude입니다. Cursor/Grok은 현재 계정 사용량이고 Copilot은 기본 Orca rate-limit provider에 없습니다.

## Copilot: 세션과 계정 quota

[GitHub SDK 문서](https://docs.github.com/en/copilot/how-tos/copilot-sdk/features/usage-and-billing)는 계정 `account.getQuota`와 세션 `session.usage.getMetrics`를 구분합니다. 이 프로젝트는 계정 quota만 읽으며 추론 세션을 만들지 않습니다. `Session 0 AIC`만으로 월간 잔여를 계산하지 않습니다.

조사 환경의 CLI 0/6,500 AIC와 SDK 0/6500, remainingPercentage=100이 일치했습니다. 영점 사례만으로 소수 AIC 사용량의 단위 대응을 검증할 수 없습니다. 백분율은 원천을 쓰되 SDK requests를 AIC/금액으로 단정하지 않습니다. 없는 quota는 확인 불가입니다.

SDK resetDate가 조회 시각처럼 반환된다는 [upstream 보고 #2619](https://github.com/github/copilot-sdk/issues/2619)도 있습니다. 조회마다 새 SDK runtime을 열어 프로세스 캐시 영향을 줄이고, 월간 reset은 [조직·기업용 공식 정책](https://docs.github.com/en/copilot/concepts/billing-and-usage/organizations-and-enterprises/billing)의 매월 1일 00:00 UTC를 정책 값으로 사용합니다. SDK가 제공한 실제 resetDate라고 주장하지 않습니다.

## Reddit·개발 커뮤니티

작성자의 경험담이며 이 장비에서 재현된 원인 진단은 아닙니다. 전기적 설정·API 판단은 공식 소스가 우선합니다.

| 경험담 | 반영한 판단 |
|---|---|
| [LCD-7 native USB/UART 인식 차이](https://www.reddit.com/r/esp32/comments/1nuzmj0/waveshare_esp32s3_7_display_issues_with_arduino/) | UART·앱 USB 설정·BOOT 모드를 분리 확인 |
| [RGB와 Wi-Fi를 함께 쓸 때 문제](https://www.reddit.com/r/homeassistant/comments/1raq7ho/waveshare_esp32s3_7_touch_lcd_running_as_a_home/) | 특정 작성자의 PCLK 변경을 보편값으로 채택하지 않음 |
| [AI quota null 레코드 선택 문제](https://www.reddit.com/r/esp32/comments/1w4ew64/esp32_eink_dashboard_for_claude_and_codex_usage/) | unknown·0·stale·offline 분리, pool/timestamp 검사 |
| [SD 접근 중 화면 깜빡임과 IO expander CS](https://forum.lvgl.io/t/screen-flickers-when-reading-from-sd-card/18310) | 초기 SD 제외, 추가 시 정확한 보드 포트 사용 |

실제 장비에서는 chip/flash 조회, 원본 백업, firmware 부팅/PSRAM, GT911 좌표·색상·tearing, BLE 승인/거절·재접속·PC 절전/종료, 최소 MTU, 비영점 Copilot 값 비교가 남아 있습니다.

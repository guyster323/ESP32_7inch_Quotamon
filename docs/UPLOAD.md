# LCD-7 업로드와 BOOT / RESET

대상: **ESP32-S3-Touch-LCD-7 Rev1.2, 800×480**. ESP32-S3-WROOM-1은 모듈명이며 이 이름만으로 보드 핀맵이나 플래시 용량을 확정할 수 없습니다. 7C/7C-BOX 예제를 혼용하지 마세요.

## 두 USB-C 포트

- **UART**: USB-UART 브리지 경로. PC에 COM 포트로 인식되어 esptool/업로드·로그에 사용합니다. 공식 안내에 자동 다운로드 회로가 기재되어 있지만 현재 연결 장비에서 자동 리셋의 실제 성공은 확인되지 않았습니다.
- **USB**: ESP32-S3 네이티브 USB 경로. 현재 앱의 USB 설정·보드 회로 상태에 따라 Windows 열거 상태가 달라질 수 있습니다. 단순히 드라이버가 없어서만 실패하는 것은 아닙니다.

2026-10-15 실물 조회에서 UART COM4는 수동 BOOT/RESET과 자동 리셋 모두 `No serial data received`로 실패했습니다. **BOOT을 누른 채 네이티브 USB 포트에 연결**한 뒤 COM5(VID 303A / PID 1001)가 나타났고, ESP32-S3 rev0.2 / **16MB flash / 8MB PSRAM**을 확인했습니다. COM 번호는 재연결 시 다시 확인하세요.

UART Type-C와 UART 헤더는 보드의 UART1/UART2 선택 스위치를 공유합니다. UART 경로가 응답하지 않으면 스위치 위치도 확인하세요. 네이티브 USB는 CAN과 핀을 공유하므로 실행 중인 예제에 따라 COM 포트가 보이지 않을 수 있습니다. BOOT 진입 후 다시 확인합니다.

## 수동 다운로드 모드

1. 데이터 케이블을 UART에 연결합니다.
2. **BOOT을 누른 채 유지**합니다.
3. **RESET을 눌렀다 놓습니다.**
4. **BOOT을 놓습니다.**
5. 칩/용량 조회 또는 업로드를 실행합니다.
6. 업로드 후 **RESET**을 눌러 새 앱을 시작합니다.

2026-10-15 네이티브 USB 업로드는 부트로더·파티션·앱 해시 검증까지 통과했습니다. USB RTS reset 후에는 다운로드 ROM에 남아 검은 화면이 계속됐습니다. **USB와 별도 전원을 모두 분리해 전원 표시등이 꺼짐을 확인하고 5초 후 아무 버튼도 누르지 않고 USB를 재연결**하자 Quotamon 화면이 표시됐으며 BLE 시작 버튼의 터치와 PC 광고 검색도 확인했습니다. RESET/USB 재부팅 후 화면이 검으면 이 전원 재연결 절차를 사용하세요.

즉, BOOT과 RESET을 단순히 동시에 누르는 대신 BOOT이 눌린 상태에서 RESET을 해제하는 순서가 중요합니다. 자동 다운로드가 성공하는 경우 매번 수동 조작할 필요는 없습니다. 부팅 핀은 BOOT의 GPIO0입니다.

이 프로젝트에는 PlatformIO가 설치한 esptool이 있습니다. `.venv`에 `esptool` 모듈이 별도로 설치되어 있다고 가정하지 않고 다음 경로를 사용합니다. 수동 BOOT 진입 직후 읽기 전용 확인 예:

```powershell
.venv\Scripts\python.exe -m serial.tools.list_ports -v
.venv\Scripts\python.exe .tools\platformio\packages\tool-esptoolpy\esptool.py --chip esp32s3 --port COM5 --baud 115200 --before no_reset --after no_reset --no-stub flash_id
```

## 펌웨어 빌드

```powershell
python tools/bootstrap_dependencies.py
python -m pip install platformio==6.1.18 intelhex==2.3.0
python -m platformio run -d firmware
```

PlatformIO 플랫폼은 `espressif32@6.13.0`(ESP-IDF 5.5 계열), LVGL 8.4.0, `esp_lvgl_adapter` 0.5.2로 고정합니다. 그래픽 어댑터는 IDF 5.5 이상이 필요합니다. 구성은 공식 LCD-7 RGB/GT911 포트와 8 MB 파티션 구성, octal PSRAM을 기준으로 하며, 확인된 16MB 보드에서도 아래 8MB 영역을 사용합니다. 보드의 실제 용량과 **최종 링크 이미지가 파티션에 들어가는지** 확인한 후 업로드하세요. 기본 설계는 OTA 슬롯이 없습니다.

개발 PC에서 ESP-IDF 5.5.3 빌드가 통과했고 계측 이미지 크기는 약 6.03 MB입니다. [현재 빌드·업로드 기록](character-animation-build.json)과 [CPU·FPS·PSRAM 실측](RUNTIME_METRICS.md)을 참조하세요. `DOUBLE_DIRECT` 모드로 RGB 버퍼 2개를 명시했습니다. CMake는 그래픽 어댑터의 forced-include를 묶어 PlatformIO의 플래그 재정렬로 header 경로가 별도 소스처럼 해석되는 문제를 방지합니다. 프로젝트 가상환경에 PlatformIO/intelhex를 실제 설치하세요. 임의 PYTHONPATH 설치는 빌더의 환경 정리로 의존성이 사라질 수 있습니다.

IDF component-manager가 구형 1.5.3으로 설치되어 최신 manifest 조건을 파싱하지 못하면 PlatformIO가 사용하는 IDF Python 환경에서 `idf-component-manager==2.5.2`로 갱신한 뒤 다시 빌드하세요. TLS 가로채기가 있는 환경에서는 운영체제에서 신뢰하는 인증서를 빌드 도구에 제공하고, 인증서 검증을 끄지 마세요.

업로드는 기존 플래시 앱을 바꾸므로, 현재 factory demo를 보존하려면 먼저 로컬 백업을 만드세요. 아직 검증되지 않은 바이너리를 다른 Waveshare 모델에 업로드하지 않습니다.

```powershell
# 실제 UART 포트와 다운로드 모드/용량을 확인한 다음
python -m platformio run -d firmware -t upload --upload-port COM4
```

공식 근거: [Waveshare LCD-7 사용 안내](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Instructions-For-Use), [Waveshare UART 선택·USB/CAN FAQ](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/FAQ), [Espressif ESP32-S3 부팅 모드 선택](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

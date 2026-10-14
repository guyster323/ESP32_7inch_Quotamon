# 검증 기록

기준: 2026-10-14. 개발 PC의 검사 결과이며 실제 보드의 작동 인증이 아닙니다.

| 검사 | 결과 |
|---|---|
| TypeScript core build / 5개 테스트 | framing, privacy/freshness, settings bounds, host/controller lifecycle, 미로그인 provider placeholder 제외 통과 |
| C++ wire 테스트 | CRC/golden frame, 24 KB, value size 20/244/512, 손상·누락 거부 통과 |
| LVGL 네이티브 UI 빌드·포인터 테스트 | 카드·상세·설정·페이지·계정 선택·페어링 승인/거절 통과 |
| 800×480 렌더 | 한글 glyph 경고 없음, 막대 경계의 두 글자색, offline 채움 삭제 확인 |
| Windows .NET 10 BLE resource | build/publish 통과, 경고 0, actual PC adapter central 지원 확인 |
| ESP32 firmware | PlatformIO 6.13.0 / ESP-IDF 5.5.3 빌드 통과; program 6,358,807 / 8,323,072 bytes (76.4%) |
| Orca 1.4.214 소스 패치 | 적용 및 reverse check 통과 |
| Orca 설정 컴포넌트 분리 렌더 | 실제 Orca UI 부품 + 모의 API로 페어링·별칭·저장/취소·주기·HTML escaping·760 px 검사 통과 |
| 기존 설치 Orca 변경 | 수행하지 않음 |
| 실행 중인 Orca 1.4.224 실제 쿼터 읽기 | 기본 + 추가 Codex 2계정, Cursor가 모두 ready; 기존 Copilot CLI SDK도 ready |
| 실제 쿼터의 BLE 형식 변환 | 5계정 JSON 약 2.5 KB, value size 20에서 약 205 frames; 메모리 내 분할/재조립 일치 |
| PC BLE 및 장치 검색 | 실제 adapter central/low-energy 지원; 20초 검색에서 Quotamon advertiser 0개 관측 |
| 전체 patched Orca build / Electron 화면 | 아직 수행하지 않음 |
| ESP32 chip/flash 조회 | UART 인식, 자동 리셋 조회는 No serial data received |
| 펌웨어 업로드 / 실제 BLE 왕복 | 아직 수행하지 않음 |

```powershell
python tools/bootstrap_dependencies.py
npm ci --prefix orca-integration
npm test --prefix orca-integration
tools\build_native.cmd
build\native\quotamon_protocol_test.exe
python -m pip install Pillow==12.3.0
python tools/verify_native_visuals.py
dotnet publish orca-integration/windows/Quotamon.Ble.csproj -c Release -o dist/orca-resource
dist\orca-resource\Quotamon.Ble.exe --probe
```

UI 테스트는 실제 공유 LVGL 코드·RGB565 자산을 사용하며 수치는 모의 데이터입니다. 실제 GT911 전기적 좌표/IRQ는 검사하지 않습니다. `visuals/native/native-checks.json`에 mock/hardware 여부를 기록합니다.

펌웨어 크기 검사는 flash partition 적합성 검사입니다. 표시된 정적 RAM 25,272 bytes는 실제 부팅 후 BLE·PSRAM framebuffer·JSON/UI의 동적 메모리 총량을 의미하지 않습니다. 2개 RGB framebuffer, malloc/PSRAM, XIP 구성을 사용하며 실제 여유 메모리·tearing·radio 동시 동작은 하드웨어에서 확인해야 합니다.

Controller 테스트는 모의 BLE resource로 마지막 write 응답보다 ACK가 먼저 도착하는 경우, 저장 revision 충돌, 종료·설정 해제를 검사합니다. Windows radio·페어링·절전 검사와는 별개입니다. 릴리스 전 실제 보드와 전체 Orca 앱 결과를 추가해야 합니다.

## 실행 중인 Orca로 재확인

설치된 Orca가 켜져 있는 PC에서 core를 빌드한 뒤 아래 진단을 실행합니다. Windows BLE 리소스가 빌드되어 있으면 adapter 확인도 수행합니다.

```powershell
npm ci --prefix orca-integration
npm run build --prefix orca-integration
node tools/verify_running_orca.mjs --copilot --scan
```

진단은 설치된 CLI의 `status`와 `account list --json`을 사용합니다. 계정 응답은 메모리에서만 변환하고, 출력은 provider별 계정 수·상태·메시지 크기만 포함합니다. 계정 이름·이메일·쿼터 수치·주소·인증정보는 출력하거나 저장하지 않습니다. 임시로 모든 계정을 선택해 데이터 변환을 검사하지만 Orca 설정을 저장하거나 BLE로 데이터를 전송하지 않습니다.

`--copilot`은 기존 Windows npm Copilot CLI의 읽기 전용 quota RPC를 검사합니다. 모델 프롬프트나 세션을 만들지 않습니다. `--scan`은 Quotamon service UUID만 20초 검색하고 페어링/연결하지 않습니다. 기본 실행은 Orca 캐시와 adapter만 읽습니다. `ready`는 쿼터 원천의 상태이며, UI가 실제 보드에 표시되었다는 의미가 아닙니다. 캐시 시각에 따라 stale로 바뀔 수 있습니다.

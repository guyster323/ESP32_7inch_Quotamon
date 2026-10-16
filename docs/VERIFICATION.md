# 검증 기록

기준: 2026-10-16. 개발 PC의 소프트웨어 검사와 실물 보드의 관측 결과를 구분해 기록합니다. 화면·기본 터치 확인을 전체 하드웨어 작동 인증으로 표현하지 않습니다.

| 검사 | 결과 |
|---|---|
| 실물 CPU·FPS·PSRAM | 2026-10-16. 계측 앱 업로드·플래시 검증·부트로더/파티션/NVS 보존 후 기존 인증 BLE 연결에서 통계 수집. 홈 100.536초/20표본: CPU 두 코어 평균 6.10%, 실제 LVGL 갱신 7.43fps. 상세 80.484초/16표본: 6.08%, 2.57fps. PSRAM 여유는 두 구간 모두 912,804바이트, 최대 연속 블록 884,736바이트. [방법·원시 표본](RUNTIME_METRICS.md). TypeScript 38개 테스트 통과. 계측 비용 포함, 장시간 누수·최악 부하 시험은 아님 |
| 캐릭터 애니메이션 | 2026-10-16. 공유 LVGL 렌더에서 5종×6상태 동작·표정, Copilot 눈꺼풀, 캐릭터 영역 밖 픽셀 불변, 상세 화면, 스냅샷 적용, 40회 화면 전환 후 힙·타이머 유지, 설정·연결 상실 시 정지 검사 통과. [GIF·구현 기록](CHARACTER_ANIMATION.md), [펌웨어 빌드 크기·해시](character-animation-build.json). 실물 앱 영역 업로드·플래시 검증·이전 앱 백업 해시와 부트로더/파티션/NVS 보존 검사 완료. PC BLE 도우미 재시작 후 사용자 화면 기동·BLE 연결·정상 동작 확인. 개별 모션의 직접 관측·별도 ACK 수집은 미수행; CPU/FPS/PSRAM 실측은 별도 행에 기록 |
| 웹 전송 전 갱신 | 2026-10-15 단독 수정. TypeScript/회귀 테스트 35개 통과. 가상 시계로 60초/90초 주기의 2초 전 알림과 ACK·설정 변경 재예약, 원천 캐시 변경 시 정기 전송 대기, 페어링 즉시 알림·인증·구독 해제 확인. 실제 웹+모의 원천/BLE 검사에서 2초 유휴 폴링 제거, 설정·검색·페어링·원천 상실 동작 통과. 최종 패키지의 실제 Orca 원천·Codex 3계정·저장 5개 ID/순서 유지 확인. 15:02 적용; 설정·자동 실행·런타임·Orca 보존 |
| TypeScript core build / 14개 테스트 | Windows 저장 페어링·실패 안내, Cursor 모델 쿼터 선택·누락 처리, 독립된 최근 갱신 시각과 축약 리셋 시간 회귀 검사 통과 |
| 계정 신원·소스 순서 | 2026-10-15 `npm test --prefix orca-integration` 34개 통과. 런타임 소유자, 같은 조직의 다른 구성원, 저장 슬롯·요금제 센티넬, 메모리 캐시, poll/provider refresh 지연 응답과 dispose, 원천이 없을 때 보드 설정 알림이 저장 선택을 바꾸지 않음을 포함. 실행 중인 Orca에서 실제 계정 전환은 하지 않음 |
| 계정 신원 스테이징 패키지 | 코디네이터가 `dist/Quotamon-Portable-account-identity`를 임시 설정(`address=null`)으로 기동. 최종 런타임 재포장과 오프라인 설정 거절 이후에도 실제 Orca 원천, Codex 3계정, 저장 5개 ID·순서, Cursor 모델 풀이 유지됐다. 보드 페어링·연결 없음. 매니페스트 19개 파일 해시 일치 |
| 계정 신원 실행 폴더 적용 | 2026-10-15 14:48 `dist/Quotamon-Portable`에 `bridge.cjs`와 `build-manifest.json`만 교체하고 Quotamon만 재시작. 설정은 첫 요금제 센티넬이 빈 값이 된 것만 달라졌고, 자동 실행·Node·`Quotamon.exe`·Orca PID/app.asar는 유지. 새 Chrome UI에서 Orca 연결됨·디스플레이 동기화 중 확인. 실물 LCD 계정 전환은 관찰하지 않음 |
| C++ wire 테스트 | CRC/golden frame, 24 KB, value size 20/244/512, 손상·누락 거부 통과 |
| LVGL 네이티브 UI 빌드·포인터 테스트 | 카드·상세·설정·페이지·계정 선택·페어링 승인/거절 통과 |
| 800×480 렌더 | 한글 glyph 경고 없음, 막대 경계의 두 글자색, offline 채움 삭제 확인 |
| Windows .NET 10 BLE resource | build/publish 통과, 경고 0, actual PC adapter central 지원 확인 |
| ESP32 firmware | PlatformIO 6.13.0 / ESP-IDF 5.5.3 빌드 통과; 최근 갱신·리셋 표시 수정본 program 6,361,131 / 8,323,072 bytes (76.4%) |
| Orca 1.4.214 소스 패치 | 적용 및 reverse check 통과 |
| Orca 설정 컴포넌트 분리 렌더 | 실제 Orca UI 부품 + 모의 API로 페어링·별칭·저장/취소·주기·HTML escaping·760 px 검사 통과 |
| 기존 설치 Orca 변경 | 수행하지 않음 |
| 실행 중인 Orca 1.4.224 실제 쿼터 읽기 | 기본 + 추가 Codex 2계정, Cursor가 모두 ready; 기존 Copilot CLI SDK도 ready |
| 실제 쿼터의 BLE 형식 변환 | 5계정 JSON 약 2.5 KB, value size 20에서 약 205 frames; 메모리 내 분할/재조립 일치 |
| PC BLE 및 장치 검색 | 실제 adapter central/low-energy 지원. 업로드 후 실물 Quotamon advertiser 검색 확인 |
| 포터블 패키지 실제 실행 | 함께 배포하는 Node + Quotamon.exe BLE 모드로 5계정 ready, adapter, 임시 설정/revision/privacy, Copilot 끄기/켜기 통과; 설치된 Orca app.asar SHA-256 변경 없음 |
| 포터블 실제 제품 UI | 모의 원천·BLE로 계정 편집/저장, pending connect 중 번호 승인, 새로고침, 760/430 px, 원천 상실 시 카드 삭제, JS/CSP 오류 없음 |
| 트레이 실행기·종료 처리 | headless 실제 실행으로 BLE 모듈 강제 종료 → 1회 복구 → 안정 유지, 실행기 강제 종료 시 모든 자식 종료 확인; 검사는 자동 실행 항목 변경 안 함 |
| 전체 patched Orca build / Electron 화면 | 아직 수행하지 않음 |
| ESP32 chip/flash 조회 | UART COM4는 무응답. 네이티브 USB COM5에서 ESP32-S3 rev0.2 / 16MB flash / 8MB embedded PSRAM 조회 성공 |
| 기존 플래시 전체 백업 | 16,777,216 bytes 비공개 로컬 백업. 전송 digest 및 원본 대상 verify_flash digest 일치 |
| 펌웨어 업로드 | 부트로더·파티션·앱 업로드 및 세 이미지 hash 검증 통과. 검증된 8MB 파티션 구성을 16MB 실물 보드에 사용 |
| 실물 LCD·기본 터치 | 전체 전원 분리 후 재연결해 Quotamon 화면 표시, BLE 연결 시작 버튼 터치를 사용자 확인 |
| 실제 BLE 페어링 | 앱 custom PairAsync는 번호 이벤트 전 Failed. Windows 기본 창에서 동일 번호·양쪽 승인 성공. Secure Connections=true / EncryptionAndAuthentication / access Allowed 확인 |
| Windows에 저장된 실물 기기 검색 | 수정한 DeviceWatcher가 광고 없는 페어링 장치를 발견함. 서비스 UUID·암호화 GATT 확인은 연결 시 수행 |
| 실제 BLE GATT·왕복 | Windows의 uncached primary-service 조회가 0x80070016을 반환해 cached fallback 추가. 서비스·2개 characteristic·notify 구독 성공, MTU value size 244. 수정 패키지의 helper와 실제 Orca/기존 5계정 선택으로 전송하고 보드 snapshot ACK를 2회 이상 확인 |
| 기존 앱 적용 | 사용자 승인 후 기존 폴더에 적용·재시작. staged/적용 파일 전체 해시 일치, 계정 5개·화면 설정·자동 실행 보존. 새 앱의 자동 연결 및 오류 없는 동기화 중 UI 재확인. 임시 중계는 종료 |
| Cursor 모델 쿼터 표시 개선 | Cursor Models·Other Models만 표시·전송하고 Monthly 제외. 실제 Orca 원천·모의 웹 UI·적용된 PC 화면 확인. 사용자가 적용 후 LCD 정상 표시 확인 |
| 최근 갱신 시각·리셋 일정 개선 | 네이티브 렌더에서 하단 최근 갱신/원천 수집 두 줄, 시각 변경·숨김·연결 상실, `Reset 5 … | W …`와 Cursor 공통 리셋, 긴 일정의 두 줄 배치 검사 통과. PC 브리지 적용 완료. 새 앱 업로드·읽기 검증, 이전 앱 백업 해시 및 부트로더·파티션·NVS 보존 검사 통과. 전원 재연결 후 사용자 화면 기동 확인, 실행 중인 PC UI의 자동 BLE 재연결·동기화 상태 확인. 실물 문구별 배치 확인 및 이번 변경의 별도 ACK 관찰은 미수행 |

```powershell
python tools/bootstrap_dependencies.py
npm ci --prefix orca-integration
npm test --prefix orca-integration
tools\build_native.cmd
build\native\quotamon_protocol_test.exe
python -m pip install Pillow==12.3.0
python tools/verify_native_visuals.py
python tools/verify_character_animation.py
dotnet publish orca-integration/windows/Quotamon.Ble.csproj -c Release -o dist/orca-resource
dist\orca-resource\Quotamon.Ble.exe --probe
```

UI 테스트는 실제 공유 LVGL 코드·RGB565 자산을 사용하며 수치는 모의 데이터입니다. 실제 GT911 전기적 좌표/IRQ는 검사하지 않습니다. `visuals/native/native-checks.json`에 mock/hardware 여부를 기록합니다.

펌웨어 크기 검사는 flash partition 적합성 검사입니다. 표시된 정적 RAM 25,272 bytes는 실제 부팅 후 BLE·PSRAM framebuffer·JSON/UI의 동적 메모리 총량을 의미하지 않습니다. 2개 RGB framebuffer, malloc/PSRAM, XIP 구성을 사용하며 실제 여유 메모리·tearing·radio 동시 동작은 하드웨어에서 확인해야 합니다.

Controller 테스트는 모의 BLE resource로 마지막 write 응답보다 ACK가 먼저 도착하는 경우, 저장 revision 충돌, 종료·설정 해제를 검사합니다. 실물 결과는 위 표에 따로 기록했습니다. 절전·모든 오류 복구 시나리오를 검증한 것은 아닙니다. 포터블 중계 사용에 전체 Orca 빌드는 필요하지 않습니다. 선택적인 내부 패치의 전체 앱 빌드는 여전히 검증 전입니다.

## 실행 중인 Orca로 재확인

설치된 Orca가 켜져 있는 PC에서 core를 빌드한 뒤 아래 진단을 실행합니다. Windows BLE 리소스가 빌드되어 있으면 adapter 확인도 수행합니다.

```powershell
npm ci --prefix orca-integration
npm run build --prefix orca-integration
node tools/verify_running_orca.mjs --copilot --scan
```

진단은 설치된 CLI의 `status`와 `account list --json`을 사용합니다. 계정 응답은 메모리에서만 변환하고, 출력은 provider별 계정 수·상태·메시지 크기만 포함합니다. 계정 이름·이메일·쿼터 수치·주소·인증정보는 출력하거나 저장하지 않습니다. 임시로 모든 계정을 선택해 데이터 변환을 검사하지만 Orca 설정을 저장하거나 BLE로 데이터를 전송하지 않습니다.

`--copilot`은 기존 Windows npm Copilot CLI의 읽기 전용 quota RPC를 검사합니다. 모델 프롬프트나 세션을 만들지 않습니다. `--scan`은 Quotamon service UUID만 20초 검색하고 페어링/연결하지 않습니다. 기본 실행은 Orca 캐시와 adapter만 읽습니다. `ready`는 쿼터 원천의 상태이며, UI가 실제 보드에 표시되었다는 의미가 아닙니다. 캐시 시각에 따라 stale로 바뀔 수 있습니다.

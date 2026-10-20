> 2026-10-20 폐기 결정: 아래는 이전 광고 스캔 구현·실물 시험의 역사적 기록이다. 다음 작업 지침으로 사용하지 않는다. 현재 방향은 [직접 페어링 전환 계획](BLE_PAIRED_PRESENCE_PLAN.md)을 따른다.

# BLE 휴대폰 감지로 화면 제어

2026-10-20 준비된 펌웨어와 PC 중계를 실물에 적용했다. ESP32의 주변 BLE 광고 검색과 PC 연결을 확인했으나, Bluetooth를 켜고 설정 화면을 연 Fold7의 이름/별칭은 수신 목록에서 확인하지 못했다. 사용자는 **휴대폰 추가 앱 없이, PC 없이, Wi-Fi 없이, Bluetooth만으로 보드가 직접 감지**하기를 원한다. 현재 구현과 관측 결과로는 이 요구를 만족시키지 못하므로 휴대폰 감지는 꺼 둔다. 2026-10-19의 모의 검증과 실물 결과를 구분한다.

## 2026-10-20 실물 적용과 현재 상태

- COM5의 ESP32-S3 rev0.2, 16MB flash / 8MB PSRAM을 확인했다. 기존 앱을 백업하고 이전 배포 해시와 대조한 뒤 `0x10000` 앱 영역만 업로드했다. 새 이미지 검증 및 `0x0~0xFFFF` 전후 비교로 부트로더·파티션·NVS 보존을 확인했다.
- `dist/Quotamon-Portable`에 준비된 PC 패키지를 적용했다. 기존 계정 선택·순서·별칭·요금제·revision·예약·페어링 주소·자동 실행은 유지한다. 새 중계가 설정을 저장하면서 기본값인 `presence_*` 4개 필드만 추가됐다.
- 사용자 확인: 휴대폰은 `선우의 Z Fold7`, Bluetooth 켜짐, 보드 옆에 있음. Bluetooth 설정 화면을 열고 재검색해도 저장 이름/영문 별칭이 보이지 않았다. 주변 기기의 광고 수신과 `scanning=true`, `available=true`는 실물에서 확인했다. 이름 없는 주소를 임의로 휴대폰으로 선택하지 않았다.
- 소프트웨어 재부팅 뒤 LCD 초기화 이후 로그와 재연결이 지연된 경우, 모든 전원 분리 후 재연결로 복구했다. 사용자가 화면 기동을 확인했으며 PC BLE 연결과 검색도 복구됐다.
- 남은 시험: 이름을 포함한 휴대폰 BLE 광고, 잠금 중 광고 유지, 60초 미감지 시 꺼짐·복귀 시 켜짐, 터치 5분 점등, 예약과의 조합, 재부팅 후 대상 복원. 아직 실제 폰 감지 성공이나 이탈·복귀 통과로 기록하지 않는다.
- 사용자 제약: 휴대폰 광고 앱 설치, PC를 통한 감지, Wi-Fi 감지는 채택하지 않는다. 현재 BLE 광고 기반 구현은 Fold7의 안정적인 식별을 입증하지 못했다. 주변의 이름 없는 주소를 신호 세기만으로 휴대폰이라고 가정하거나, 식별자 변경 가능성을 무시하고 기능을 켜지 않는다.
- 홈 화면 실측 9개 표본·45.166초: CPU 두 코어 평균 7.214%, LVGL 갱신률 7.793fps, 가용 PSRAM 1,966,912바이트. 이 수치는 검색 종료 뒤 BLE 연결 상태의 표본이며, 연속 검색 중 성능이나 휴대폰 감지 성공을 뜻하지 않는다. [실물 기록](ble-presence-hardware.json).
- 상세 플래시 백업·검색 원문은 비공개 `.local/board-ble-presence-20261020/`, PC 적용 전 백업은 `.local/before-ble-presence-20261020-074837/`에 보관한다. 주변 주소와 계정 설정은 공개 검증 기록에 넣지 않는다.

현재 보드의 ESP32-S3는 Bluetooth LE만 지원하며 Bluetooth Classic은 지원하지 않는다. 따라서 일반 Bluetooth 이름 검색이나 Classic 서비스 응답 확인을 이 보드에 그대로 추가할 수는 없다. [Espressif 지원 범위](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/bt-architecture/overview.html).

### 휴대폰 직접 페어링 검토

사용자가 대안으로 보드와 휴대폰의 직접 페어링을 제안했다. **ESP32-S3 하드웨어와 NimBLE는 복수 BLE 연결을 지원하지만 현재 펌웨어는 지원하지 않는다.** 실제 빌드의 `CONFIG_BT_NIMBLE_MAX_BONDS=1`, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`이며, `ble_transport.cpp`도 연결 핸들·인증·알림 구독·프레임 재조립 상태를 각각 하나만 관리한다. 설정 숫자만 늘리면 PC와 휴대폰을 안전하게 분리할 수 없다. 기존 PC 페어링을 보존해야 하므로 현재 버전에서 새 휴대폰 페어링을 시도하지 않았다.

이 경로를 진행하려면 PC 데이터 연결과 휴대폰 감지 연결을 분리하고, 휴대폰의 기본 Bluetooth UI를 통한 페어링 가능 여부, 잠금 중 연결 유지, 이탈 후 자동 재연결을 실물에서 확인해야 한다. 페어링 정보 저장만으로 현재 근접 여부를 증명할 수는 없다. 폰의 이름 광고가 안 잡힌다는 사실만으로 이 별도 연결 경로까지 불가능하다고 결론 내리지 않는다. 현재는 구현·검증 전이다. [Espressif 복수 연결 안내](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32s3/api-guides/ble/ble-multiconnection-guide.html).

추후 제약 변경 시 검토할 대안은 (1) 고정 식별자를 광고하는 BLE 태그를 휴대폰에 부착, (2) 폰에서 이름 포함 BLE 광고를 송신, (3) PC에서 Classic Bluetooth 실응답을 확인, (4) 같은 Wi-Fi에서 감지하는 방식이다. 어느 대안도 현재 선택·구현·실물 검증한 것으로 취급하지 않는다. 폰 광고 앱의 기술 참고만 보관한다: [Nordic Advertiser 안내](https://github.com/nordicsemi/Android-nRF-Connect/blob/main/documentation/README.md#advertiser).

## 사용 방법

- LCD: **설정 → 화면 끄기 예약 → 휴대폰 감지**. **선택한 기기가 있을 때만 화면 켜기**를 체크하고 **주변의 BT기기 리스트**에서 기기를 선택한 뒤 **저장**한다.
- PC 포터블 설정: **설정 → 휴대폰 감지**에서 같은 체크박스와 검색 버튼을 사용한다. 검색은 PC가 아니라 연결된 ESP32에서 20초 동안 수행한다.
- 기본 대상 이름은 `선우의 Z Fold7`, 기본 별칭은 `seonui`, `seonui Z Fold7`이다. 기능 기본값은 꺼짐이며 사용자가 체크·저장하면 시작한다.
- LCD에서는 **같은 폰의 다른 이름 추가** 체크 후 목록을 선택하고, PC에서는 **다른 이름 추가** 또는 줄 단위 입력으로 별칭을 최대 4개 등록한다. 별칭 추가는 주 기기의 선택을 바꾸지 않는다.
- 선택한 주소는 이름 없는 광고나 동일 주소의 이름 변경에도 사용한다. 주소가 바뀌면 저장된 이름/별칭의 완전 일치로 감지한다. ASCII 대소문자와 양끝 공백은 무시하지만, 부분 문자열·자동 로마자 변환은 사용하지 않는다.

## 화면 제어

| 조건 | 동작 |
|---|---|
| 선택한 기기의 광고 수신 | 예약 시간대가 아니면 화면 켜기 |
| 마지막 감지 후 60초 경과 | 화면 끄기 |
| 부팅·기능 활성화·대상 변경 | 처음 60초 동안 화면 유지 |
| 기존 화면 끄기 예약도 사용 | 예약 시간대이거나 폰이 없으면 끄기 |
| 꺼진 화면 터치 | 기존 밝기로 5분간 켜기 |
| BLE 스캐너 오류·복구 중 | 감지 조건으로 화면을 끄지 않으며, 복구 후 60초 유예 |
| PC 연결 해제 | 저장된 대상 감지는 ESP32에서 계속 수행 |

ESP32의 기존 PC 연결·보안 페어링·heartbeat는 유지한다. 휴대폰에 연결하거나 페어링을 요청하지 않는다. 스캐너는 Observer 역할을 사용하고 중복 광고를 허용해 마지막 감지 시각을 계속 갱신한다. 스캔 목록은 최대 24개, 이름은 UTF-8 96바이트, 목록의 최근 광고 기준은 20초다.

## 휴대폰에서 확인할 사항

이 기능은 **BLE 광고를 실제로 송신하는 기기**를 감지한다. Android Bluetooth 설정의 일반 기기 이름이 항상 BLE 광고에 포함되는 것은 아니며, Bluetooth를 켜는 것만으로 이름 광고가 보장되지 않는다. 내일 우선 Fold7이 잠금 상태에서도 어떤 이름/주소로 광고되는지 확인해야 한다. 이름 없는 랜덤 주소만 주기적으로 바뀌면 페어링 없이 그 폰을 계속 동일하게 식별할 수 없으므로, 폰에서 일관된 이름의 BLE 광고를 보내는 구성이 필요하다.

근거: [Android BLE 광고 API](https://developer.android.com/reference/android/bluetooth/le/BluetoothLeAdvertiser), [NimBLE GAP 스캔 API](https://mynewt.apache.org/v1_8_0/network/ble_hs/ble_gap.html).

## 다국어 표시와 저장

기기명은 받은 UTF-8 문자열을 그대로 표시하고 저장한다. 잘못된 UTF-8은 대체 글리프로 처리하고 제어 문자는 제거하며 코드포인트 중간을 자르지 않는다. PC에서는 텍스트 노드로 삽입해 기기명을 HTML로 실행하지 않는다.

LCD UI는 한국어·일본어·중국어 간체·독일어·영어를 지원한다. UI에 필요한 글자는 기존 14/16/20px로 유지하고, 임의 한글은 공통 16px 폰트, BMP 중국어 한자/확장 A·가나·전각 문자는 공통 16px 보조 폰트를 사용한다. 폰트 압축과 한글 공유로 플래시/XIP PSRAM을 절약한다. 보조 평면 희귀 한자·이모지 전체의 글리프를 포함하지는 않는다. 문자열 저장·전송은 이 경우에도 UTF-8을 유지한다.

기기 설정은 `quotamon-ui` NVS의 `presence` 문자열에 저장한다. 쓰기는 기존처럼 메인 DRAM 작업에서 수행한다. PC 설정·스냅샷에는 `presence_enabled`, `presence_address`, `presence_name`, `presence_aliases`를 사용하며 기존 revision 충돌 검사를 적용한다. 구형 저장 설정은 기능 꺼짐으로 읽고, 구형 펌웨어가 새 필드를 생략한 설정 변경을 보내면 현재 PC 값을 유지한다.

## 실물 시험 순서

1. 준비된 펌웨어/포터블 빌드와 검증 기록을 확인하고 기존 동작 중인 프로그램·보드의 업데이트 상태를 확인한다. 현재 배포본은 오늘 자동으로 교체하지 않았다.
2. 펌웨어 업로드와 새 PC 중계 적용 후, **휴대폰 감지 기능은 끈 상태에서 먼저 검색**한다. PC와의 BLE 연결 및 수치 갱신을 유지하는지도 확인한다.
3. `선우의 Z Fold7` 또는 실제 수신된 영문 이름을 선택한다. 필요한 이름만 별칭으로 추가하고 체크·저장한다.
4. 폰을 옆에 놓은 상태, 잠금 상태, 자리를 비운 뒤 60초 경과, 복귀 시 화면 켜짐을 각각 확인한다. 폰이 광고하지 않는 상황을 이탈과 혼동하지 않는다.
5. 예약과 감지를 함께 켰을 때, 터치로 5분간 켜기, 전원 재연결 후 대상 복원, PC 재연결, 주소 변경 후 별칭 감지를 확인한다.

## 검증 파일

- `npm test --prefix orca-integration`: 설정 유효성·UTF-8 저장/전송·ESP32 검색 요청·잘못된 응답 거부·연결 해제 후 목록 삭제 포함.
- `build/native/quotamon_ble_presence_test.exe`: 주소/별칭 일치, 잘못된 UTF-8, 60초 경계, 라디오 오류 복구, 예약/터치 우선순위.
- `build/native/quotamon_ui_fonts_test.exe`: 5개 언어 UI 및 기기명 글리프.
- `python tools/verify_ble_presence_ui.py`: 실제 공통 LVGL 및 PC UI의 체크/선택/별칭/저장/취소, 스캔 갱신 중 선택 안정성, HTML 이름, 좁은 화면.
- [LCD 결과](../visuals/presence/lcd-checks.json), [PC 결과](../visuals/presence/web-checks.json), [한국어 PC 예시](../visuals/presence/web-ko.png), [일본어 LCD 기기 목록](../visuals/presence/lcd-ja-devices.png).
- 최종 펌웨어 크기·해시·추정 PSRAM 여유는 `ble-presence-build.json`에 기록한다. 실제 무선/런타임 성능 측정과 구분한다.
- `python tools/verify_ble_presence_build.py`: 최종 이미지의 Observer·폰트 압축 설정, 플래시·XIP PSRAM 예산, PC 패키지 파일 해시를 검사하고 펌웨어 파일을 스테이징한다.

준비된 PC 패키지: `dist/Quotamon-Portable-BLE-presence-win-x64.zip`.

2026-10-19 최종 결과: TypeScript 테스트 48개, CTest 5개, 5개 언어 LCD/PC UI 검사 통과. 별도 임시 설정으로 실제 스테이징 PC 번들/Node 실행도 확인했다. 펌웨어는 **4,956,048 / 8,323,072바이트**, LCD 버퍼·XIP·LVGL 스택을 반영한 추정 PSRAM 여유는 **1,921,024바이트**다. 실제 실행 시 남는 힙 측정값은 아니다. 이미지 해시와 PC 파일 검증 결과는 [빌드 기록](ble-presence-build.json)에 저장했다.

준비된 펌웨어: `dist/Quotamon-BLE-presence-firmware/`의 `firmware.bin`, `bootloader.bin`, `partitions.bin`, `build-verification.json`. 현재 `.pio` 빌드와 동일한 파일이며 업로드하지 않았다. 내일 적용할 때는 기존 [업로드 안내](UPLOAD.md)와 현재 실제 포트를 확인하고 NVS·PC 설정을 유지한다.

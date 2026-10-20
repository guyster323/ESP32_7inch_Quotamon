# Quotamon 세션 인수인계

## 최신: D 구현·적용과 핵심 실물 확인 완료, E 일부 미검증

현재 보드 D SHA-256 `5f87fb711e7de9a2e0b207a17505ec8319ebd5ba7fa28accef23fef4cbbc7d31` (4,929,840바이트), 설치 PC 0.3.0 실행 파일 `a81332786632461ed1482872614be2fe9d7140f76ee4e8404f5c50cf05bd21de`이다. 실행 경로는 기존 `dist/Quotamon-Portable`이며 자동 실행 명령은 유지했다. 광고 스캔·주소/별칭 설정·검색 API·목록을 제거하고 보드 소유 설정과 schema2 읽기 전용 PC 상태로 바꿨다. 표시 ID는 임의16바이트이며 인증 권한을 주지 않는다. 공통 UTF-8은 bluetooth_name.h에 분리했다.

펌웨어/네이티브 빌드, TypeScript49개, CTest8개(C 역할의 표시ID 마이그레이션·유지·저장 실패 시 본딩 보존 포함), 5개 언어 LCD/PC 웹과 430px/HTML 이름/저장·취소/구형 API 폐기 검사를 통과했다. D 업로드와 플래시 읽기, 업로드 전후 부트로더·파티션·NVS 동일도 통과했다. [D 빌드 기록](ble-paired-migration-build.json), [D 실물 기록](ble-paired-migration-hardware.json)을 따른다. 별도 배포본은 `dist/Quotamon-Paired-Portable`, 펌웨어는 `dist/Quotamon-BLE-paired-migration-firmware`다.

사용자가 D 콜드부트 후 사용량/휴대폰 자동 연결을 확인했고, 실제 ACK1건과 PC schema2 연결/감지 켜짐 스크린샷을 확보했다. 임시 진단 중계에서 트레이로 전환하면서 잠깐 사용량 대기 화면이 생겼고 자동 복구했다(PC t130199 해제→147239 재인증). 이후 트레이를 계속 유지한75초 표본6개에서 BLE/표시5와 가용 메모리가 안정적이었다. 진단 전환을 제품의 지속 장애로 기록하지 않으며 해당 사용자 보고도 숨기지 않는다. 현재 트레이 중계는 정상 실행 중이다.

사용자는 예약 실물 시험을 지금 하기 어렵다고 답했다. 임시 예약은 적용하지 않았고 **기존 예약 켜짐18:00~07:00/revision7**이 유지된다. 안내한23:00 복원은 기본값 가정 오류였고 실제 저장값으로 정정했다. 설치 설정은 폐기한 네 광고 필드만 제거한 뒤 이전과 의미상 동일하다. 원래 PC 전체/설정 백업은 `.local/before-stage-d-apply-20261020-140216`, 직전 C 앱/NVS는 `.local/board-ble-paired-trial-20261020-140204`이다. 키가 들어 있는 백업은 공개하지 않는다.

B 잠금30분·입력 정상·실제 거리 복귀1회는 통과 이력이고 추가4회를 요구하지 않는다. C f43에서 사용량 자동 복구·해제60,088ms 후 화면OFF·자동인증66ms 후ON·터치300,044ms 후OFF를 통과했다. 이미지를 구분해 근거를 보존하며 D에서 전부 재실행한 것으로 쓰지 않는다.

남은 E 실물: 예약 우선순위, 로컬 감지 스위치 재부팅 복원, PC 완전 종료 상태의 독립 동작, 폰 재부팅 후 복구, 선택 삭제/재등록과 더 긴 안정성. 사용자 시험 가능 시 재개한다. PC를 임의 종료하거나 전체 본딩을 지우지 않는다. 새 플래시는 현재 필요 없다. 자체 passive COM5 캡처89587은 종료해 포트를 해제했고 관찰18944도 끝났다. 필요하면 새 수동 캡처를 시작한다. 상세 기준과 결과는 [계획18절](BLE_PAIRED_PRESENCE_PLAN.md#18-c-실물-결과와-d-설정ui-전환)을 따른다.

## 이전 단계별 이력

아래는 당시 상태를 보존한 과거 기록이다. ‘업로드 전’, ‘B 실행 중’, ‘미검증’ 문구가 최신 상태를 덮어쓰지 않는다.

## C단계 업로드·읽기 검증 완료 · 부팅/화면 실물 시험 전

C단계 화면 제어 시험본 `f43a9658caf76c0d3d8cd200ae05bfa3892af09a67ec15f40fa34fcf6aea6be7` (4,978,912바이트)을 앱 영역에 업로드하고 플래시 읽기 검증을 마쳤다. 현재 앱·본딩은 `.local/board-ble-paired-trial-20261020-132504/`에 새로 백업했고 이전 앱은 B 이미지 `2a4…`와 일치했다. 업로드 전후 부트로더·파티션·NVS가 동일하며 PC 실행 파일·중계·manifest·설정 해시도 유지됐다. C 실물 기록은 [ble-paired-presence-hardware.json](ble-paired-presence-hardware.json)이다. 이제 모든 전원을 5초 분리한 뒤 BOOT 없이 연결해 부팅·본딩 자동 복구를 확인한다. 이어 BT 끄기→연결 해제부터 60초 후 LCD 끄기→BT 켜기 자동 재연결과 켜짐을 시험한다. 업로드 성공을 LCD 실물 동작 통과로 간주하지 않는다. D의 구형 광고 스키마/PC UI 전환은 아직 남아 있다.

## C단계 시험본 준비 완료 · 업로드 전

C단계 소프트웨어 검증 완료: 펌웨어·네이티브 빌드, CTest 8개, TypeScript 48개, 5개 언어 감지 스위치/상태 화면 검사를 통과했다. 이미지 SHA-256은 `f43a9658caf76c0d3d8cd200ae05bfa3892af09a67ec15f40fa34fcf6aea6be7`, 4,978,912바이트다. 산출물은 `dist/Quotamon-BLE-paired-presence-firmware`, 기록은 `ble-paired-presence-build.json`/`ble-paired-presence-hardware.json`으로 B 이력과 분리한다. 현재는 업로드 전이며 B 이미지가 실행 중이다. 다음은 새 BOOT/RESET 준비 확인 → 현재 앱/NVS 새 백업 → `tools/flash_ble_paired_trial.py --stage C --port COM5 --baseline-build docs/ble-paired-trial-build.json` 적용과 읽기 검증이다. PC 설치본과 설정은 유지한다. 적용 후 BT 끄기→연결 해제부터 60초 후 LCD 끄기→BT 켜기 자동 연결과 화면 켜기를 우선 확인한다. 예약·터치 5분·PC 종료 상태의 독립 동작·감지 스위치 저장은 별도 실물 항목이며 아직 통과하지 않았다. 추가 먼 거리 왕복 4회를 요구하지 않는다.

## 17. C단계 구현 시작: paired 상태와 LCD 제어

사용자가 다음 구현을 명시적으로 요청했다. 7절 B 종료 조건인 기존 PC 본딩·사용량/ACK 보존, Fold7 기본 설정 등록·인증, 잠금 30분 유지, 사용자 변경 기준에 따른 실제 거리 복귀 1회는 로그와 사용자 확인을 모두 확보했으므로 **B 연결 게이트를 통과**한 것으로 정리한다. 앞서 남겨 둔 PC 완전 종료 상태의 독립 동작은 7절 C에 정한 구현 후 실물 검증이며, C 구현 자체를 막는 선행 조건으로 해석하지 않는다. PC 종료 상태·화면 끄기/켜기 등 미검증 결과는 유지한다.

C 소스는 인증된 폰 링크 기반 상태 모델과 연결 해제 시각부터 60초 유예, 재연결 취소, 라디오 오류 시 감지로 화면을 끄지 않기와 복구 후 새 유예, 대상 삭제/변경, 예약 우선·터치 5분 점등을 구현 중이다. 보드 NVS의 `paired_on`과 LCD 스위치로 감지를 제어하며 구형 PC 광고 설정은 새 감지를 켜거나 끄지 못한다. 검증된 HID/PC GATT 속성 순서는 유지한다. 현재 보드에는 여전히 B 이미지가 실행 중이며, C는 빌드·모의 검사 및 별도 산출물 준비 후 업로드한다. 광고 필드/API 전면 제거와 PC 상태 UI 마이그레이션은 D단계로 남긴다.

## 최신 확인: 실제 거리 이탈·복귀는 1회로 충족

사용자가 이동 거리 부담 때문에 반복 기준을 5회에서 1회로 수정하도록 명시했다. 실제 거리 이탈 후 generation41 연결 해제와 generation42 인증/HID 구독 복구를 사용자 결과와 대조해 1회 통과로 기록했다. 추가 4회를 요구하지 않는다. 30분 연속 잠금과 PC 보완 후 LCD 사용량 정상도 사용자 확인을 받았다. 화면 끄기는 현재 시험본에서 비활성이다. PC 완전 종료 상태 등 미검증 요구는 유지하며 전체 하드웨어 통과를 주장하지 않는다. 최신 기준은 [계획 16절](BLE_PAIRED_PRESENCE_PLAN.md#16-사용자-변경-실제-이탈복귀-시험-1회로-충족)과 실물 JSON을 따른다.

## 2026-10-20 PC 기본 기능 복구와 잠금 시험 진행

Fold7 기본 설정 페어링·인증·HID 구독과 사용자 기본 키보드/입력 정상 확인을 마쳤다. 휴대폰 잠금 시험은 11:37:21 KST 시작, 최소 종료 12:07:21이며 사용자 확인은 12:08 이후다. 보드 재부팅·펌웨어 변경 없이 계속 관찰 중이다.

사용자 지적대로 PC 사용량 갱신이 멈췄고 인증 약 4초 뒤 재접속을 반복했다. 휴대폰 handle1과 PC handle2가 동시에 인증돼 BLE 독점은 아니었다. 진단에서 Windows의 Uncached 서비스/항목 검색 Unreachable·0x80070016을 확인했다. 저장된 PC 본딩은 캐시를 우선 사용하고 항목 검색도 같은 예외를 처리하도록 `orca-integration/windows/Program.cs`를 보완했다. 새 페어링의 검색·인증 암호화·UUID 검증은 유지한다. 빌드·적용한 실행 파일 SHA-256은 `b0f73ac6205dde9d5b9a58c7161813faba6589471df007c9bc226888904a69a5`이며 이전 파일과 manifest는 `.local/pc-repair-before-20261020-120053`에 백업했다. PC 설정·자동 실행 등록·보드 펌웨어는 유지했다.

설치된 helper와 bridge를 단일 PC 클라이언트로 실행해 계정 5개·실제 ACK 3건·통계 수신을 확인하고 원래 트레이 실행을 복구했다. 동시에 띄우는 두 번째 관찰 helper는 여전히 실패하므로 이를 주 클라이언트의 ACK 검증으로 사용하지 않는다. 기존 ZIP은 이번 실행 파일 교체를 반영하지 않았으며 현장 적용은 dist/Quotamon-Portable/Quotamon.exe다. 사용자가 30분 연속 잠금과 LCD 사용량 정상 표시를 확인했다. 실제 거리 이탈·복귀는 사용자 변경 기준인 1회를 로그와 대조해 충족했다. 추가 4회는 요구하지 않는다. PC 완전 종료 상태 등 아직 확인하지 않은 실물 항목과 C/E의 실제 화면 끄기·켜기는 미검증으로 유지한다.

## 최신 상태: 사용자 선택에 따른 HID 시험본 업로드·플래시 검증 완료

사용자가 **“입력 장치로 등록하는 HID 시험 진행”**을 선택했다. 이 방향을 다시 승인받지 않는다. [계획 14절](BLE_PAIRED_PRESENCE_PLAN.md#14-사용자-선택-hid-상호운용-시험-진행)에 따라 NimBLE/PC 서비스를 유지하며 HID 소비자 제어 서비스, Device Information, 측정 불가를 명시하는 Battery companion 서비스를 구현했다. 키보드·마우스 컬렉션과 자동 HID 입력 전송은 없다. 번호 비교·PC/폰 역할·저장·권한을 유지한다. 임의 배터리 백분율과 다른 업체의 제품 ID를 사용하지 않으며 적합성 인증은 주장하지 않는다.

HID 소스의 펌웨어/네이티브 빌드, 실제 BLE 전송 소스의 HID 형식·권한·Suspend·입력 알림 미발생을 포함한 CTest 7개, 5개 언어 LCD 등록 화면 조작 검사를 통과했다. 스테이징 이미지 SHA-256은 `2a4d9adfd7f560b541dbe6979eff4a9b5006bdc56e22d5ef0647803081d50ba1`, 4,972,816바이트이며 예상 PSRAM 여유는 1,921,024바이트다. **업로드·실물 상태는 아래 과거 기록이 아닌 현재 [빌드 기록](ble-paired-trial-build.json)·[실물 기록](ble-paired-trial-hardware.json)의 이미지 해시별 근거로 확인한다.**

사용자의 새 BOOT/RESET 준비 완료 후 COM5를 재확인하고 현재 앱·NVS를 `.local/board-ble-paired-trial-20261020-112421/`에 백업했다. 이전 앱은 광고 보완본 `20ef…`와 해시가 일치했다. HID 시험본 `2a4…`의 앱 영역만 업로드하고 플래시 검증을 마쳤으며 업로드 전후 부트로더·파티션·NVS도 동일했다. 이전 이미지·기록은 `.local/paired-trial-before-hid/`에 보관했다. PC 프로그램·설정은 교체하거나 재시작하지 않았다. 일반 GATT 콘솔 55717은 종료했으며 새 수동 조작 없는 HID 콘솔은 세션 10571이다. 다음 업로드가 필요하면 콘솔을 먼저 종료해 포트를 해제한다. 전원 전체 분리·5초 대기·BOOT 없이 재연결한 뒤 Fold7 기본 설정의 검색·양쪽 번호 비교·인증된 링크·키보드 영향·잠금 유지·자동 복귀 시험이 필요하다. B단계 통과 전 C/D/E는 진행하지 않는다.

## 이전 일반 GATT 시험 결론: B단계 기본 설정 검색 실패

광고 보완본 `20ef2472ad210ad410a494bf0fe9d1cfeab5e5d819ec81e2e9e2dd29950b9b60`에서도 Fold7 기본 Bluetooth 목록에 Quotamon이 없다. 휴대폰 Bluetooth 끄기/켜기·등록 창 재개 후에도 연결 가능/등록된 목록 모두 없다고 사용자가 확인했다. 정확한 저장 보드 주소를 내부 대조한 22초 검색에서 실제 무선 광고 22회와 저장 본딩 열거 1회를 구분했고 이름·서비스 UUID·RSSI -77~-56 dBm을 확인했다. 보드 로그에서도 PC 연결 전후 광고 활성, 기존 PC 인증과 새 런타임 통계 수신은 정상이며 휴대폰 접속은 없었다. 원인을 Samsung 내부 필터로 확정하지 않는다. **현재 일반 GATT의 네이티브 등록 경로는 미통과이며 C/D/E로 진행하지 않는다.**

[계획 13절](BLE_PAIRED_PRESENCE_PLAN.md#13-광고-보완본의-실물-게이트-결과와-표준-프로파일-검토)은 당시의 표준 프로파일 후보와 HOGP 시험 범위다. 이후 사용자 선택과 실제 HID 구현·업로드 상태는 위 최신 기록 및 계획 14절을 따른다. 직접 PC ACK는 미검증이며, 이전 일반 GATT 콘솔 `.local/paired-trial-console/20261020-101303.log`의 수집은 종료했다.

## 2026-10-20 B단계 후속: Fold7 검색 실패와 광고 보완본

초기 시험본은 LCD 기동과 기존 PC의 인증 재접속·런타임 통계 수신까지 확인했다. 그러나 사용자가 휴대폰 등록 시험을 열어도 Fold7 기본 Bluetooth 연결 가능 목록에 Quotamon이 보이지 않았다. 등록 버튼 입력은 로그에 두 번 기록됐으며 휴대폰 접속은 없었다. 원인은 아직 확정하지 않았다.

광고 보완본은 기본 광고 31바이트 안에 완전한 이름과 기존 서비스 UUID를 넣고, 고정 광고 데이터는 호스트 동기화 때 설정한 뒤 PC 접속 후에는 광고만 재개한다. 광고 설정/응답/시작 오류와 등록 창의 광고 상태를 기록하고 오류 재시도를 1초로 제한했다. 펌웨어 빌드와 실제 전송 소스를 포함한 CTest 7개를 통과했다. 보완본 SHA-256은 `20ef2472ad210ad410a494bf0fe9d1cfeab5e5d819ec81e2e9e2dd29950b9b60`, 크기는 4,969,904바이트다. 업로드 상태와 이 이미지의 실물 검증 상태는 [새 빌드 기록](ble-paired-trial-build.json), [새 실물 기록](ble-paired-trial-hardware.json)에서 확인한다. 이전 시험본과 기록은 `.local/paired-trial-before-advertising/`, 현재 보드의 새 앱·NVS 백업은 `.local/board-ble-paired-trial-20261020-100539/`에 있다. PC 설치본을 교체하지 않는다. 광고 수정만으로 Fold7 문제가 해결됐다고 주장하지 않으며 B단계 실물 게이트 전에는 C/D/E를 진행하지 않는다.

## 2026-10-20 후속 구현: B단계 직접 페어링 시험본

진행 기준은 [직접 페어링 계획 12절](BLE_PAIRED_PRESENCE_PLAN.md#12-2026-10-20-후속-구현-상태)이다. 소스에는 PC/휴대폰의 독립 연결·본딩 역할, PC 전용 데이터 권한, 대상별 번호 비교, 선택 삭제와 LCD 시험 UI를 구현했다. 펌웨어의 광고 스캐너와 감지에 의한 화면 끄기는 비활성이다. 공통/PC의 광고 설정·API 전면 제거는 실물 게이트 이후 D단계에서 진행한다.

TypeScript 48개, CTest 7개, 5개 언어 LCD 조작 모의 검사를 통과했다. CTest에는 실제 BLE 전송 소스를 모의 호스트·저장소에서 실행하는 권한/다중 연결 회귀 검사가 포함된다. 실물 결과와 혼동하지 않는다. 사용자의 BOOT/RESET 준비 후 현재 앱·NVS를 백업·검증하고 새 시험본의 앱 영역 업로드·플래시 검증을 마쳤다. 업로드 전후 부트로더·파티션·NVS 및 기존 PC 설치본·설정이 같은 것도 확인했다. 전원 재연결 후 부팅·PC ACK와 폰 시험을 이어간다. [새 빌드 기록](ble-paired-trial-build.json), [새 실물 기록](ble-paired-trial-hardware.json)을 사용한다. **B단계의 Fold7 기본 설정 연결·잠금 유지·자동 복귀 재연결은 아직 통과하지 않았다.**

전원 전체 분리·5초 대기 후 LCD 기동을 사용자가 확인했고, 앱 부팅 로그와 기존 PC 본딩의 인증 재접속을 확인했다. 기존 PC 중계가 새 런타임 통계를 받고 있으며 설치본·설정 해시도 그대로다. 별도 GATT 관찰기는 `operation-failed`로 직접 ACK를 확보하지 못했으므로 ACK와 폰 시험은 미검증이다. LCD 휴대폰 등록 시험과 Fold7 기본 Bluetooth 등록 결과를 사용자에게 요청했다.

## 2026-10-20 계획 확정 당시 기준: 휴대폰 감지를 직접 페어링 방식으로 전환

**다음 세션은 [BLE 직접 페어링 구현계획](BLE_PAIRED_PRESENCE_PLAN.md)을 먼저 읽는다.** 사용자가 기존 휴대폰 BLE 광고 스캔 방식의 폐기와 직접 페어링 방식 전환을 결정했다. 이번에는 계획만 작성했으며 실제 전환 구현은 새 세션에서 진행한다.

- 요구: 휴대폰 추가 앱·PC를 통한 감지·Wi-Fi 없이, 보드가 등록한 휴대폰의 실제 BLE 연결 상태로 감지한다. PC 1대와 폰 1대의 본딩/연결을 분리하며 기존 PC 사용량 전송을 보존한다.
- 현 설치는 이전 광고 스캔 버전이고 감지 기능은 꺼져 있다. COM5 업로드/플래시 검증, PC 재연결·주변 광고 검색·사용자 화면 기동은 확인했다. Fold7 이름은 수신 목록에서 확인하지 못했고 폰 직접 페어링·이탈/복귀 성공은 미검증이다.
- 하드웨어는 멀티 BLE 연결을 지원하지만 실제 펌웨어의 최대 본딩/연결은 각각 1이며 전역 연결 상태를 공유한다. 숫자만 늘리지 말고 역할·연결 컨텍스트·보안·저장을 분리한다.
- 첫 기술 검증은 Fold7 기본 Bluetooth UI의 페어링, 잠금 중 연결 유지, 복귀 시 자동 재연결이다. 본딩만으로 근접 판정하지 않는다. 이 경로가 OS에서 가능한지 확인하기 전에 전체 기능 성공을 약속하지 않는다.
- 현재 앱 SHA-256은 `b7834a938f3fea3095769961d007c8fb5c94754d394d27ded07b13facf038423`, 실행 폴더는 `dist/Quotamon-Portable`이다. 기존 PC 본딩·설정·자동 실행을 보존한다. [빌드 기록](ble-presence-build.json), [실물 기록](ble-presence-hardware.json), [휴대폰 감지 안내](BLE_PRESENCE.md)를 참조한다.
- 원본 백업은 `.local/board-ble-presence-20261020/`, PC 적용 전 백업은 `.local/before-ble-presence-20261020-074837/`에 있다. 새 세션에서 실행 상태·실제 포트를 재확인한다. 소프트웨어 재부팅 후 LCD 기동이 지연되어 전체 전원 재연결로 복구한 전례가 있다.
- 현재 작업 트리에는 기존 미커밋 변경이 많다. 광고 스캔 제거 시 관련 변경만 교체하고 다른 작업을 초기화하지 않는다. 폰 광고 스캔과 Windows의 Quotamon 보드 검색을 혼동해 PC 검색까지 없애지 않는다.

## 이전 인수인계 기록

작성 기준: **2026-10-15 / Windows / `D:\AI_Driven\ESPmonitor`**

> 새 세션은 이 문서를 먼저 읽는다. **실물 플래시 전체 백업·펌웨어 업로드·Windows 기본 번호 비교 페어링·실제 5계정 전송과 보드 ACK·사용자의 5카드 표시 확인을 완료했다.** PC의 저장 페어링 검색과 Windows GATT 조회 호환성을 수정해 기존 Quotamon 폴더에 적용했다. 이후 Cursor를 두 모델 쿼터만 표시하도록 개선해 적용·재시작했고, **사용자가 LCD 정상 표시도 확인했다.** 새 앱은 저장된 보드에 자동 연결되어 동기화 중이다. Orca와 보드는 재시작하지 않았다. 현재 실행 중인 Orca와 Quotamon을 임의로 종료하거나 재설치하지 않는다.

## 2026-10-15 후속 변경: 웹 갱신 주기

- 단독 구현. 웹의 2초 `setInterval` 폴링을 없애고 인증된 `/api/events` 스트림으로 알림을 받는다. 마지막 전송 ACK와 설정 주기를 기준으로 다음 전송 2초 전에 한 번 상태를 읽는다. 60초 설정은 ACK 후 58초에 읽는다.
- 원천 캐시 변경은 다음 정기 전송에 반영한다. 최초 화면, 페어링·검색·연결/원천 상태 변화, 사용자 동작은 즉시 표시하고, 설정 저장·수동 갱신·연결의 즉시 전송은 유지한다. 검색 종료는 한 번의 로컬 타이머로 버튼만 갱신한다.
- 테스트 35개와 실제 웹 UI의 모의 원천/BLE 검사가 통과했다. 60/90초 주기의 전송 2초 전 알림, ACK·주기 변경에 따른 재예약, 불필요한 원천 변경 알림 억제, 페어링 즉시 표시, 인증·구독 해제를 확인했다.
- 15:02에 `bridge.cjs`, `web/app.js`, `build-manifest.json`만 기존 `dist/Quotamon-Portable`에 적용하고 Quotamon을 재시작했다. 설정 파일 바이트·자동 실행·Node·런처·Orca는 그대로다. 백업은 `.local/before-web-schedule-20261015-150200`이다.

## 2026-10-15 후속 변경: 계정 신원

- Codex/Claude 실시간 사용량은 런타임 소유자에게만 붙는다. 기본 슬롯은 `system-default`이고, 관리 계정은 각 관리 ID다. 같은 조직이거나 provider 계정 ID가 같다고 한 사용자로 합치지 않는다.
- 마지막 ready 값은 프로세스 메모리에만 둔다. 원천 상실, 원천 갱신 실패, dispose에서 지운다. 쿼터와 인증은 디스크에 저장하지 않는다.
- 저장된 ID·별칭·요금제·순서는 유지한다. 원천에서 빠진 선택 행은 그 자리에 확인 불가로 남는다. 저장값 `요금제 미확인`은 빈 요금제로 정규화하고, 사용자가 저장한 요금제가 provider plan보다 우선한다.
- 한 풀만 보이는 보드 카드의 요금제 문구와 “터치해서 상세 확인”은 그대로다. 이번 변경은 펌웨어를 수정하거나 업로드하지 않았다.
- `orca-integration` 테스트 34개가 통과했다. 실행 중인 Orca에서 계정을 실제로 전환하지는 않았다. 마지막 ready 값은 메모리에만 있으므로, 기본 계정이 선택되지 않은 채로 앱이 새로 시작되면 그 카드는 확인 불가로 시작한다.
- 2026-10-15 14:48에 검증된 `bridge.cjs`와 `build-manifest.json`만 `dist/Quotamon-Portable`에 적용하고 Quotamon만 다시 시작했다. Orca와 보드는 재시작하지 않았다. 백업은 `.local/before-account-identity-20261015-144810`이다.
- 저장 계정 5개의 ID·순서·별칭·사용 여부, revision, 화면 설정, 페어링 주소, 자동 실행 등록, Node와 `Quotamon.exe`는 유지됐다. 첫 계정의 저장 요금제 `요금제 미확인`만 빈 값으로 정규화됐다.
- 코디네이터는 새 Chrome 설정 화면에서 Orca 연결됨과 디스플레이 동기화 중을 확인했다. 이것은 소프트웨어 UI 검증이며 실물 LCD의 계정 전환 관찰은 아니다.
- 원천이 없는 동안 들어온 보드 설정 알림은 저장하지 않는다. 저장 ID·별칭·요금제·순서는 그대로 남는다.

## 2026-10-15 후속 변경: 최근 갱신 시각·리셋 일정

- 사용자는 첫 카드의 원천 수집 시각과 별도로 최근 갱신 시각을 하단에 표시하고, 리셋 일정을 `W`/`5`, `d`/`h`/`m`으로 줄이도록 요청했다. 5시간 카드에도 주간 리셋을 함께 표시하며, Cursor는 공통 `Reset W` 한 번만 표시한다.
- 하단은 상태 줄과 `최근 갱신 HH:mm:ss | 첫 카드 수집 HH:mm:ss` 두 줄이다. `refreshed`는 PC에서 스냅샷을 생성한 현지 시각이며, 계정의 원천 수집 시각과 별개다. 설정으로 숨기기·연결 상실 시 지우기를 유지한다.
- 카드에는 `Reset 5 2h 41m | W 5d 3h`처럼 표시한다. 폭을 넘으면 두 줄로 나누고 막대·이미지 크기를 조정해 겹치지 않도록 한다. Cursor 상세에서도 모델별 중복 리셋을 제거했다.
- TypeScript 14개, 네이티브 LVGL 화면·포인터·프레이밍 검사와 새 펌웨어 빌드를 통과했다. 모의 화면은 `visuals/native/01-home.png`, 긴 일정의 두 줄 예시는 `18-wrapped-reset.png`다.
- PC 브리지는 `dist/Quotamon-Portable-refresh-time/`의 검증된 파일을 현재 실행 폴더에 적용·재시작했다. 원본 설정 파일과 자동 실행 등록을 보존했다.
- **사용자 준비 응답 후 COM5로 새 앱 업로드·읽기 검증을 완료했다.** 이전 앱을 추가 백업해 기존 SHA256 일치를 확인하고, 0x10000의 앱만 교체했다. 0x0~0xFFFF를 업로드 전후 읽어 부트로더·파티션·페어링 NVS가 바뀌지 않았음을 확인했다. 새 이미지 SHA256은 `cf62574f83d4e0dc400d996e9f96d248390782fdfa68f15e11607ea1408526f9`다.
- 모든 전원을 분리해 5초 후 BOOT 없이 재연결한 뒤 사용자가 **화면 기동**을 확인했다. 실행 중인 Quotamon 설정 화면에서도 Orca 연결 및 **디스플레이와 동기화 중** 표시로 자동 BLE 재연결을 확인했다. 새 시각·리셋 문구의 세부 배치는 네이티브 화면 검사로 검증했으며 실물에서의 문구별 확인 응답은 아직 없다. 상세 로그와 이전 앱·보존 영역 백업은 `.local/board-footer-reset-update/`에 있다.
- 사용자가 이번 후속 변경의 커밋·푸시를 요청했다. 이번 커밋에는 최근 갱신 시각·축약 리셋 일정·두 줄 배치와 펌웨어 적용·검증 기록을 함께 포함한다. 직전 Cursor/BLE 수정 커밋 `b51d797`은 원격 main에 반영됐다.

## 2026-10-15 후속 변경: Cursor 모델 쿼터 표시

- 사용자 요청에 따라 Cursor는 **Cursor Models → Other Models** 두 쿼터만 표시·전송한다. Monthly 등 다른 풀은 제외하며, 모델 풀이 누락되면 해당 항목을 확인 불가로 유지한다.
- PC 대시보드에서도 두 항목을 모두 렌더링한다. 현재 펌웨어는 두 풀 표시를 지원하므로 재업로드하지 않았다.
- TypeScript 테스트 12개와 모의 웹 UI 검사 통과. 실제 Orca의 두 모델 풀을 확인했고, 현재 실행 폴더에 `bridge.cjs`, `web/app.js`, 빌드 매니페스트를 적용한 뒤 Quotamon만 재시작했다. 설정 파일·자동 실행 등록은 보존했다.
- 실행 중인 설정 화면에서 두 모델 이름과 **디스플레이와 동기화 중** 상태를 확인했고, 이후 사용자가 **LCD도 정상**이라고 확인했다. 별도 ACK 관찰기는 `operation-failed`로 종료되어 이번 변경의 새 ACK는 직접 검증하지 못했다.
- 패키지: `dist/Quotamon-Portable-cursor-models-win-x64.zip`. 상세 기록은 `docs/portable-build.json`의 `cursor_models_update`를 참조한다. 교체 파일 백업은 `.local/before-cursor-models-*`에 있다.

## 1. 확정된 방향

```text
기존 Orca (수정하지 않음)
  → 설치가 필요 없는 Windows Quotamon 포터블 브리지
  → BLE 암호화 연결
  → ESP32의 독립적인 LVGL 화면·터치 UI
```

- 사용자는 Orca 프로그램 수정 없이 읽어오는 방식을 승인했다. **Orca 소스 수정·교체는 기본 경로가 아니다.**
- ESP32는 화면과 BLE 수신을 자체 실행한다. 구독 데이터를 제공하려면 근처 PC에서 **Orca와 포터블 브리지**가 실행되어야 한다. PC 없이 서비스에 직접 로그인하는 구조는 구현하지 않았다.
- Wi-Fi를 사용할 수 없는 환경이므로 BLE를 사용한다. ESP32-S3는 Bluetooth Classic/SPP를 지원하지 않는다.
- USB UART는 전원·업로드·로그 용도다.
- 사용자 PC에 설치된 Orca **1.4.224**를 그대로 사용하며 실행 파일·`app.asar`를 수정하지 않았다.
- `orca-integration/orca/`, `orca-integration/patches/`의 과거 내부 통합 자료는 선택적인 개발 참고용이다. 전체 Orca 포크 빌드는 검증하지 않았고 현재 제품에 필요하지 않다.

## 2. 사용자 요구사항

- 하드웨어: **Waveshare ESP32-S3-Touch-LCD-7, Rev1.2, 800×480**. 쉴드의 ESP32-S3-WROOM-1은 모듈명이다. **7C/7C-BOX와 혼동하지 않는다.**
- 기본 계정 5개: Orca에 로그인된 Codex 3개 + Cursor Ultra 개인 + Copilot Business 회사 계정.
- Codex 프로필 이름: Orca 기본값 / Personal / Team. 사용자가 설명한 플랜은 각각 Business Premium / Plus / Business standard다. 소스가 프로필 구분을 확실히 제공하지 않으면 임의로 계정을 매핑하지 않는다.
- Codex·Cursor의 추가 사용 결제는 꺼져 있다.
- Claude·Grok 호환, 온보딩·설정에서 계정 선택·별칭·플랜·순서 조정.
- 한 화면에 5개 카드(3+2), 터치 시 상세. 최대 20계정·계정당 8개 사용량 풀을 다룬다.
- F108 프로젝트의 픽셀 에셋과 백색/어두운색 분할 퍼센트 바를 참고했다. Copilot은 자체 제작한 6상태 픽셀 로봇 에셋을 사용한다.
- 기본 갱신 60초, 설정에서 15~3600초. 표시 항목·밝기 등도 설정 가능.
- 잔여 20% 경고, 10% 위험. 값 없음·무제한·오래된 데이터는 별도 상태로 표시한다. 작은 양수는 `<1%`로 표시한다.
- PC 종료·절전·연결 상실 시 값을 유지하지 않고 연결 대기로 전환한다. 하트비트 10초, 유효 통신 35초 부재 시 화면 데이터 삭제.
- Windows 로그인 시 자동 실행, 트레이에서 설정.
- Copilot 사용자 관측: `/usage`의 Plan **0 / 6,500 AIC**, 매월 1일 리셋. `Session: 0 AIC used`는 세션 사용량이다. **현재 사용량으로 간주하면 안 된다.**
- Copilot SDK의 요청 단위와 AIC 간 비영(非零) 값 매핑은 미검증이다. 현재 구현은 신뢰 가능한 잔여 비율을 표시하고 불확실한 AIC/금액 환산은 숨긴다. 월초 리셋은 1일 00:00 UTC 정책을 사용한다.

## 3. 저장소 및 완료 상태

- 저장소: <https://github.com/guyster323/ESP32_7inch_Quotamon>
- 브랜치: `main`
- 기능 구현 커밋: **`20df759cc12775521a0bb3972b554f87d013dcec`** — `Add portable BLE bridge for unmodified Orca`
- 해당 커밋은 원격에 push 완료. 이 인수인계 문서 작성 직전 작업 트리는 깨끗했다.
- 이번 커밋에는 실물 세션의 검색·GATT 조회 수정, Cursor 모델 쿼터 표시 개선과 검증 기록을 함께 포함한다.
- CI **38592932774**의 protocol / Windows / firmware 3개 작업 모두 성공:
  <https://github.com/guyster323/ESP32_7inch_Quotamon/actions/runs/38592932774>
- CI artifact: **`Quotamon-Portable-win-x64-preview`**. GitHub Release는 아직 만들지 않았다.
- README 한·영문, 아키텍처·BLE·업로드·조사·검증 문서도 포터블 기본 구조로 정리했다.
- 이 문서는 이후 새로 작성한 인수인계 파일이다. 다음 세션에서 `git status`로 문서 자체의 커밋 여부를 확인한다.

## 4. PC에서 현재 실행 중인 프로그램

**`D:\AI_Driven\ESPmonitor\dist\Quotamon-Portable\Quotamon.exe`**

- 2026-10-15 사용자가 **수정본 적용 및 Quotamon 재시작**을 명시적으로 승인했다. 기존 런처와 소유 프로세스 종료 후 검증된 `dist/Quotamon-Portable-pairing-fix/`의 파일을 기존 경로에 적용했다. 계정 선택·화면 설정·자동 실행 등록이 보존됐고, 성공한 Windows 페어링 주소만 연결 대상으로 저장했다. 이전 버전 유지 지시는 이번 승인 범위에서만 해제됐다.

- 실제 사용자용 트레이 앱이 실행 중이다. 이번 인수인계 시점에도 해당 경로의 실행을 확인했다.
- 동일 이름의 BLE 보조 프로세스가 함께 존재할 수 있다. 프로세스 이름·개수만 보고 중복 오류로 판단하지 않는다.
- 트레이 아이콘 더블클릭 또는 실행 파일 재실행으로 브라우저 설정 화면을 연다.
- 기존 Orca·이 앱을 테스트 정리 목적으로 종료하지 않는다. 실행 중인 폴더에 빌드 결과를 덮어쓰지 않는다.
- 사용자 설정: **`%APPDATA%\Quotamon\settings.json`**. 기본 5개 계정을 선택해 두었다. 인증정보와 실제 사용량 스냅샷을 저장하지 않는다.
- 현재 사용자 자동 실행: **`HKCU\Software\Microsoft\Windows\CurrentVersion\Run` → `Quotamon`**.
- 등록 명령은 현재 포터블 폴더의 실행 파일 절대 경로와 `--minimized`다. 폴더를 이동하면 새 위치에서 자동 실행 설정을 다시 적용해야 한다.

배포용 로컬 파일:

- 폴더: `dist/Quotamon-Portable/` — **폴더 전체가 필요하다.**
- 수정 ZIP: `dist/Quotamon-Portable-pairing-fix-win-x64.zip` — **86,971,192 bytes**
- SHA256: `1db59df6a76b84c0fed69725cb84cd4953dd499e029dc47d9ba3269b63074d30`
- 이전 ZIP `dist/Quotamon-Portable-win-x64.zip`(86,972,779 bytes / SHA256 `5f0dfec96651ae0e411cd5f19e44c2d5776b3b37880cc5600666f2e236532fd5`)은 원본 그대로 보존했다. 별도 출력 경로의 빌드가 이전 ZIP을 덮어쓰지 않도록 빌드 스크립트도 수정했다.
- 빌드 기록: [portable-build.json](portable-build.json)
- Node와 .NET 런타임을 포함하므로 사용자에게 별도 설치를 요구하지 않는다.
- 로컬 패키지는 Node 24.19.0, CI는 Node 22 계열을 사용하므로 CI ZIP과 로컬 ZIP의 해시가 다를 수 있다.
- **`dist/Quotamon/`은 이전 Python 시제품이다. 현재 제품으로 배포하지 않는다.** 기존 파일을 일괄 삭제할 필요도 없다.

## 5. 어디까지 검증했나

| 항목 | 상태와 한계 |
| --- | --- |
| TypeScript 테스트 | 12개 통과. Windows 저장 페어링 선택·연결 및 실패 안내, Cursor 모델 쿼터 선택·누락 처리 회귀 검사 포함 |
| 실제 Orca 연동 | Codex 3 + Cursor 1 + Copilot 1 모두 ready 확인. CLI 캐시 읽기와 런타임 캐시 갱신 확인 |
| Orca 무수정 | 연동 검사 전후 설치된 `app.asar` SHA256 동일 |
| 실제 PC BLE | Central/LE 어댑터 사용 가능. 펌웨어 업로드 후 실물 Quotamon 광고 발견 |
| 포터블 실행기 | BLE 보조 프로세스 종료 시 1회 복구 후 안정화. 실행기 종료 시 소유 프로세스 정리 확인 |
| 설정 웹 UI | 실제 제품 UI를 모의 데이터·모의 무선으로 검증. 계정 선택/저장/취소·PIN 승인/거절·좁은 화면·소스 상실 처리 확인 |
| LVGL 네이티브 미리보기 | 공유 UI 렌더링과 C++ 프로토콜 검사 통과 |
| 펌웨어 | ESP-IDF 빌드 성공. 2026-10-15 COM5로 업로드 완료, 부트로더·파티션·앱 해시 검증 통과. 전원 재연결 후 Quotamon 화면 확인 |
| 실제 BLE 페어링 | 앱 custom PairAsync는 번호 이벤트 전에 Failed. Windows 기본 Bluetooth 창에서는 번호 일치·양쪽 승인 성공 |
| 실제 BLE 계정 전송 | 수정 패키지 helper로 실제 선택 5계정 전송 및 보드 snapshot ACK 6회 확인. 사용자 5카드 표시 확인 |
| 수정 앱 적용 | 사용자 승인 후 기존 경로에 적용. 원본 설정·자동 실행 보존 및 새 앱의 자동 연결·동기화 중 UI 확인 |
| Cursor 모델 쿼터 표시 | Cursor Models·Other Models만 표시·전송. PC UI와 실제 Orca 데이터 검증, 적용 후 LCD 정상 표시 사용자 확인 |
| 실제 LCD·터치·PSRAM | Quotamon 화면 표시 및 BLE 시작 버튼 터치는 사용자 확인. 색상·찢어짐·좌표 정밀도·PSRAM 부팅 메모리 테스트 로그는 아직 미검증 |

공개 스크린샷은 모의 데이터다. PC UI 결과는 `visuals/portable/`에 있다. 통과한 소프트웨어 검사를 하드웨어 동작 확인으로 표현하지 않는다.

## 6. 다음 세션의 우선 작업: 실물 보드

2026-10-15 후속 세션 확인 결과:

- 사용자 BOOT/RESET 준비 응답을 받은 뒤 UART COM4에서 수동·자동 리셋 모두 조회했으나 `No serial data received`였다.
- **BOOT을 누른 채 보드의 네이티브 USB 포트에 케이블을 연결**하도록 안내했다. 최초 열거 직후에는 COM이 없었지만 잠시 뒤 **COM5 / VID 303A / PID 1001**로 나타났다. 재연결 직후 한 번의 포트 목록만 보고 실패로 단정하지 않는다.
- esptool 4.11.0 조회 성공: **ESP32-S3 rev0.2, 16MB flash, embedded 8MB PSRAM, quad flash 3.3V**.
- **플래시 전체 16,777,216 bytes 백업 완료**: `.local/board-backup-20261015/factory-full-16MB.bin`. `read_flash` 전송 digest 검사 및 별도 `verify_flash` 원본 digest 비교 모두 통과. 비공개 로컬 백업이다.
- 기존 검증된 바이너리(아래 SHA256)를 **8MB 파티션 구성 그대로** 16MB 보드에 업로드했다. 0x0 부트로더 / 0x8000 파티션 / 0x10000 앱, 세 이미지 모두 `Hash of data verified` 확인. 전체 flash erase는 하지 않았다.
- USB RTS reset은 `boot:0x0 (DOWNLOAD(USB/UART0))`에 남았다. 기본 RTS·esptool의 S3 hard reset도 같은 상태였다. watchdog reset은 USB 연결이 끊기는 동안 시리얼 설정 오류로 종료됐고 앱 실행을 입증하지 못했다.
- 사용자는 RESET 후 검은 화면을 보고했다. 이후 **모든 전원을 분리해 표시등이 꺼짐을 확인 → 5초 후 BOOT 없이 USB 재연결**해 화면이 표시됐다고 응답했다. 이어 **Quotamon 화면의 BLE 연결 시작을 터치했다**고 확인했다. USB 모드 전환/다운로드 후 검은 화면만으로 LCD 포트 오류로 단정하지 않는다.
- 실행 중인 Quotamon의 실제 PC 설정 UI에서 **Quotamon BLE 광고 발견**을 확인했다. PC UI의 동적 장치 목록에서 자동 클릭은 `stale element frame`으로 실패해 사용자에게 PC의 연결 버튼 및 양쪽 번호 비교·승인을 요청했다. 페어링 결과는 아직 기다리는 상태다.
- USB에서 수집한 로그는 다운로드 ROM 로그뿐이다. 새 앱의 PSRAM/RGB/GT911 초기화 로그는 아직 확보하지 못했으며, 화면 표시를 상세 부팅 로그 검증으로 표현하지 않는다.
- 실제 실행 중인 Quotamon 설정 화면에서 기존 5계정 조회를 확인했으며 Orca·Quotamon을 종료하거나 교체하지 않았다.
- UART 선택 스위치는 공식 FAQ상 UART1(Type-C CH343)/UART2(헤더) 경로를 전환한다. 이번 UART 실패 원인으로 확정한 것은 아니다. USB/CAN 핀 공유도 공식 FAQ에 기재되어 있다.
- `.venv`에는 독립 `esptool` 모듈이 없다. **`.venv/Scripts/python.exe .tools/platformio/packages/tool-esptoolpy/esptool.py`** 경로가 동작한다. 기존 `python -m esptool` 예시는 이 환경에서 실패한다.
- 로컬 로그: `.local/board-backup-20261015/{read-flash,verify-flash,upload,quotamon-boot}.log`. 장치 주소가 포함될 수 있으므로 공개 저장소에 추가하지 않는다.

페어링 이후의 진단·수정 결과:

1. 앱에서 2분 허용 창 직후 재시도해도 실패했다. 별도 helper로 `device-found: paired=false, kind=AssociationEndpoint, can_pair=true` → `pair-begin` → `pair-result: Failed, protection=None`을 확인했다. `PairingRequested`는 발생하지 않았다. AEP를 새로 조회해도 같은 결과였다. Bluetooth 관련 서비스는 실행 중이었다. 객체 종류나 보드의 숫자 인증 오류로 확정하지 않는다.
2. **Windows 기본 Bluetooth 설정 → 장치 추가 → Bluetooth → Quotamon**에서는 사용자가 **양쪽 번호 일치·양쪽 승인 완료**를 확인했다. 보드는 재부팅하지 않았다. 이후 Windows에는 연결됨인데 기존 앱 검색에는 안 나타나는 문제가 관측됐다.
3. 기존 검색은 광고만 받았다. Windows가 연결을 유지해 보드 광고가 중단되면 목록이 비었다. `DeviceWatcher`로 Windows에 저장된 Quotamon 페어링도 검색하도록 수정해 실물 재발견을 확인했다. 서비스 UUID와 암호화/authenticated GATT 확인은 연결 시 계속 수행한다.
4. 저장 페어링은 `Secure Connections=true`, `EncryptionAndAuthentication`, `access=Allowed`였다. Uncached primary-service 조회가 **0x80070016** COMException을 반환했다. 이 오류에 한해 Windows cached service 조회로 재시도하도록 수정했다. 실제 서비스 1개·특성 2개·notify 구독 및 value size 244를 확인했다. 모든 오류에 대해 캐시를 무조건 쓰도록 바꾼 것은 아니다.
5. 실패 시 소유 연결 해제, 페어링 실패 결과 코드 전달, Windows 기본 설정 연결 안내·링크, 저장된 기기 표시를 추가했다. 새 TypeScript 회귀 테스트 2개와 기존 8개, 모의 제품 UI 검사 모두 통과했다.
6. `.local/live_display_check.mjs`로 새 패키지 helper·기존 실제 Orca·선택된 5계정을 이용해 실물 전송 및 snapshot ACK 6회를 확인했다. 이 임시 중계는 사용자 앱을 유지했고 설정 저장을 하지 않았다. 사용자가 5카드를 확인한 다음, 수정본 적용·Quotamon만 재시작을 승인했다.
7. `.local/apply_pairing_fix.ps1`은 staged manifest의 전체 파일 해시를 확인하고, 기존 런처 1개와 그 소유 프로세스가 종료된 뒤 기존 폴더에 적용했다. 설정은 `.local/settings-before-pairing-fix-*.json`에 백업했고 계정/화면/자동 실행 보존을 검사했다. 성공한 보드 주소만 연결 대상으로 저장했다. 임시 중계와 진단 프로세스는 종료했다. 새 설정 페이지는 현재 `127.0.0.1:65410`이나 포트는 재시작마다 달라진다. 이전 `55955` 페이지는 사용하지 말고 트레이에서 새 설정을 연다.
8. 별도 진단 helper는 `.local/ble-diagnostics/publish/Quotamon.Ble.exe`다. 단일 EXE이므로 DLL이 있다고 가정하지 않는다. `run_diagnostic.py`는 비공개 `command.json`으로 scan/connect/confirm/stop을 받는다. PIN은 로그에 저장하지 않는다. `.local/observe_installed_ack.mjs`의 추가 수신 관찰 연결은 실패했으며, 그 검사를 통과했다고 표현하지 않는다. 실제 ACK 근거는 6번의 임시 실물 검증이다.

Cursor 변경 적용 후 LCD 정상 표시는 사용자가 확인했다. 추가 하드웨어 검증 항목:

1. 상세·설정 왕복, 터치 좌표·색상·찢어짐·밝기, PSRAM/RGB/GT911 부팅 로그·여유 메모리를 추가 확인한다.
2. 본딩 재연결과 연결 상실 후 데이터 삭제를 확인한다. 사용자 요청 없이 현재 Orca를 종료하거나 PC를 절전시키지 않는다.
3. 작은 MTU의 실물 전송과 실제 비영 Copilot 사용량의 CLI/SDK 의미는 아직 별도 검증이 필요하다.

이번 후속 세션의 저장소 변경은 BLE 연결 호환성·Cursor 쿼터 표시 개선과 README·업로드·검증·빌드 기록·이 인수인계 문서다. 사용자가 현재 상태의 커밋·푸시를 요청했다. 공개 문서에 실제 주소·PIN·계정 쿼터를 넣지 않았다. 별도 진단 소스·로그·백업은 `.local/` 아래에만 있다. 데스크톱은 다른 작업에서도 사용 중이므로 브라우저 상태를 읽기 전에 Quotamon 페이지인지 확인한다.

상세 절차: [UPLOAD.md](UPLOAD.md), [PORTABLE_BRIDGE.md](PORTABLE_BRIDGE.md), [VERIFICATION.md](VERIFICATION.md).

## 7. 주요 구현 위치

| 경로 | 역할 |
| --- | --- |
| `portable/Launcher.cs` | 트레이·단일 인스턴스·자동 실행·프로세스 수명 관리 |
| `portable/web/` | 로컬 브라우저 설정·계정 선택·페어링 승인 UI |
| `orca-integration/src/running-orca-source.ts` | 기존 Orca CLI 캐시 및 로컬 런타임 갱신 |
| `orca-integration/src/portable-main.ts` | 소스·설정·컨트롤러·네이티브 보조 프로세스 연결 |
| `orca-integration/src/portable-server.ts` | 로컬 HTTP와 접근 보호 |
| `orca-integration/src/quotamon-controller.ts` | BLE 전송·주기·ACK·설정 동기화·소스 상실 처리 |
| `orca-integration/src/quota-snapshot.ts` | 제공자 정규화·익명 ID·표시값 필터 |
| `orca-integration/src/copilot-quota-source.ts` | 이미 설치된 Copilot SDK의 사용량 조회 |
| `orca-integration/windows/Program.cs` | Windows WinRT BLE·숫자 비교·암호화 |
| `firmware/` | ESP-IDF·NimBLE·Waveshare RGB/GT911 포트 |
| `native/` | 공유 LVGL UI·JSON 처리·네이티브 미리보기 |
| `protocol/` | BLE 프레임·검증 코드 |

실제 경로·부품 상세는 [ARCHITECTURE.md](ARCHITECTURE.md), [BLE_PROTOCOL.md](BLE_PROTOCOL.md)를 참조한다.

## 8. 빌드와 도구 사용 메모

작업 루트는 `D:\AI_Driven\ESPmonitor`다. 변경하지 않았다면 완료된 검사를 모두 반복할 필요는 없다.

포터블 소스 검사·빌드:

```powershell
npm ci --prefix orca-integration
npm test --prefix orca-integration
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_portable.ps1 -Dotnet D:/AI_Driven/ESPmonitor/.tools/dotnet/dotnet.exe
```

- 프로젝트 .NET SDK는 **`.tools/dotnet/dotnet.exe` 10.0.402**. 시스템 SDK 6으로 대체하지 않는다.
- `tools/verify_portable_launcher.ps1`은 실행 중인 사용자 앱이 없는 상태를 전제로 한다. 지금 재실행하면 기존 앱 설정을 열거나 사용자 실행에 영향을 줄 수 있으므로 불필요하게 반복하지 않는다.
- 실제 소스 점검: `python tools/verify_portable_runtime.py` — 임시 설정, 페어링·업로드 없음.
- 모의 UI 점검: `python tools/verify_portable_ui.py`.

펌웨어 빌드:

```powershell
$env:PLATFORMIO_CORE_DIR = 'D:\AI_Driven\ESPmonitor\.tools\platformio'
.venv\Scripts\python.exe -m platformio run -d firmware
```

- 프로젝트 가상환경 PlatformIO **6.1.18**, intelhex **2.3.0**. `PYTHONPATH` 임시 설치만으로 대체하면 하위 빌더가 모듈을 찾지 못한다.
- 플랫폼 `espressif32@6.13.0`, 실제 로컬 IDF **5.5.3**, LVGL **8.4**.
- IDF Python: `.tools/platformio/penv/.espidf-5.5.3/Scripts/python.exe`.
- 구형 component-manager의 manifest 해석 문제 발생 시 해당 IDF 환경의 `idf-component-manager==2.5.2`를 확인한다.
- 기업 TLS 환경용 공개 인증서 묶음은 `.local/build-ca.pem`. 필요할 때 프로세스별 CA 환경변수를 지정하고 TLS 검증을 끄지 않는다.
- forced-include 플래그 재배열 문제는 프로젝트 CMake에서 처리했다. 관리되는 벤더 파일을 직접 수정하지 않는다.
- 현재 바이너리: `firmware/.pio/build/waveshare-lcd7/firmware.bin`, **6,359,216 bytes**.
- SHA256: `5b45231c7158d111e2f90d79476598e27b280ebf0339d7cd9d23e8930d72b271`.
- 읽기 조회는 프로젝트 Python의 esptool을 사용한다. 아래 명령은 **수동 준비 후** 사용하며 설치 버전의 도움말과 실제 COM 포트를 확인한다.

```powershell
.venv\Scripts\python.exe .tools\platformio\packages\tool-esptoolpy\esptool.py --chip esp32s3 --port COM5 --baud 115200 --before no_reset --after no_reset --no-stub flash_id
# 백업·용량 검증과 물리적 준비를 완료한 다음에만:
.venv\Scripts\python.exe -m platformio run -d firmware -t upload --upload-port COM5
```

네이티브 빌드: `tools\build_native.cmd`. 공유 UI 검증: `python tools/verify_native_visuals.py`.

## 9. 데이터 접근·보안·작업 경계

- Orca 캐시 읽기는 15초 간격, 원본 쿼터 갱신 요청은 최대 15분 간격이다. 디스플레이 전송 주기와 원본 서비스 갱신 주기는 서로 다르다.
- 런타임 접근 토큰은 PC 메모리에만 둔다. 서비스 인증파일을 복사하거나 ESP32에 넘기지 않는다. 모델 추론 세션을 생성하지 않는다.
- 로컬 웹 설정은 127.0.0.1 임의 포트·프로세스별 키·Host/Origin 검사·CSP를 사용한다. 외부에서 접속하는 서버가 아니다.
- Copilot은 PC에 이미 설치된 SDK/CLI를 이용한다. 그 SDK를 ZIP에 재배포하지 않는다.
- 원본 계정 ID는 제공자별 해시로 정규화한다. 선택하지 않은 계정의 쿼터·시간·금액은 BLE로 보내지 않는다.
- 오래된 원본(30분), 조회 실패, 리셋 지난 값은 stale 처리한다. 확인 불가능한 잔여량을 0%나 100%로 만들지 않는다.
- 공개 저장소·스크린샷·다음 세션 메모에 토큰·이메일·실제 계정 ID·BLE 주소·실제 쿼터 스냅샷을 남기지 않는다.
- `.local/continuity.md`에는 상세 작업 기록이 있지만 과거의 Orca 포크 방향도 포함되어 있다. **현재 방향은 이 문서 1절이 우선한다.**
- Orca 관리 상태를 조작할 필요가 생기면 현재 설치 버전에 맞는 `orca-cli` 스킬을 먼저 읽는다. 에이전트를 호스팅하는 Orca를 종료하지 않는다.
- 이전 세션의 요청은 기록 저장이었으나, 2026-10-15 후속 사용자 요청은 이 문서를 참고해 실물 작업을 이어가는 것이다. 사용자 BOOT/RESET 준비 응답을 받고 백업 후 업로드까지 진행했다. 기존 Orca·Quotamon 종료는 요청되지 않았다.

## 10. 주요 참고 자료

- [프로젝트 README](../README.md)
- [포터블 사용법](PORTABLE_BRIDGE.md)
- [검증 기록](VERIFICATION.md)
- [하드웨어 업로드](UPLOAD.md)
- [조사 자료](RESEARCH.md)
- [F108 참조 프로젝트](https://github.com/guyster323/F108pro_Usage_monitor)
- [Orca usage tracking](https://www.onorca.dev/docs/agents/usage-tracking)
- [Orca 소스](https://github.com/stablyai/orca)
- [Waveshare LCD-7 공식 사용법](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Instructions-For-Use)

새 세션 시작 요청 예시:

> `docs/SESSION_HANDOFF.md`를 읽고 이어서 진행해줘. 기존 Orca와 실행 중인 Quotamon은 유지해줘. 보드 업로드·Windows 기본 페어링·실제 5계정 전송과 ACK·수정 앱 적용은 완료했으니, 현재 5카드 유지 상태와 상세·설정 왕복부터 확인하자.

## 2026-10-19 BLE 휴대폰 감지 작업

사용자가 폰을 들고 자리를 비웠으며 실제 폰이 있는 환경의 시험은 내일 진행하도록 요청했다. 오늘은 구현·빌드·모의 검증·패키지 준비까지 완료하고, 보드 업로드와 실행 중인 PC 중계 교체는 보류한다. 작업 범위·검증·재개 절차는 [BLE_PRESENCE.md](BLE_PRESENCE.md), 최종 빌드는 ble-presence-build.json을 확인한다. 대상은 선우의 Z Fold7, 영문 별칭 seonui / seonui Z Fold7. 기능 기본값은 꺼짐이다. 내일은 먼저 잠금 상태에서도 이름이 포함된 BLE 광고를 실제로 송신하는지 확인해야 한다.

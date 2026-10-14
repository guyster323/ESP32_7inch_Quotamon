# Orca 통합과 온보딩

현재 일반 Orca 배포판은 Quotamon BLE 전송을 지원하지 않습니다. 이 문서는 **Orca 소스 체크아웃**을 빌드하는 개발자용입니다. 설치된 Orca를 수정하거나 인증 파일을 다른 계정/ESP32로 복사하지 않습니다.

## 검토 기준

- Orca commit: `3f81541188b435aabc822aaf36c6f23a70e1a783`, package version 1.4.214.
- `getAccountsSnapshot()`, `onAccountsChanged()`, `refreshAccountsForMobileSubscriber()`를 재사용합니다.
- 소스 패치의 적용은 확인했습니다. 전체 Orca 빌드·실제 BLE 연결은 아직 확인하지 않았습니다. 다른 Orca 버전에서는 먼저 diff와 API를 검토해야 합니다.

## 빌드

Quotamon 저장소에서 .NET 10 SDK로 Windows 리소스를 빌드합니다. self-contained 리소스이므로 최종 사용자는 별도 .NET 설치가 필요하지 않습니다.

```powershell
dotnet publish orca-integration/windows/Quotamon.Ble.csproj -c Release -o dist/orca-resource
python tools/apply_orca_integration.py --orca-root D:\source\orca
```

설치 스크립트는 기준 커밋, `git apply --check`, 기존 모듈 변경 여부를 확인합니다. 통합 모듈·설정 패널·preload API를 복사하고, 리소스를 `resources/quotamon/Quotamon.Ble.exe`에 놓습니다. 패키지 빌드는 이를 `resources/bin/`에 포함합니다. 별도 수집기/서비스/트레이를 설치하지 않습니다.

이후 Orca 저장소 자체의 AGENTS.md, 빌드·타입·품질 검사 절차에 따라 **Windows Orca 전체를 빌드**하세요. 이 저장소의 core 테스트 통과가 전체 Orca 빌드 통과를 의미하지 않습니다. 개발 앱과 UI 테스트는 `ORCA_BACKGROUND_LAUNCH=1`로 실행하며 기존 사용자 Orca를 종료·교체하지 않습니다.

다른 커밋은 스크립트가 기본 거절합니다. 소스 검토 후 필요하다면 `--allow-other-commit`을 사용할 수 있지만, 패치가 적용된다는 사실만으로 호환성이 확인되지는 않습니다.

## 첫 연결

1. LCD-7 펌웨어를 준비하고 [업로드 문서](UPLOAD.md)로 정확한 보드·용량·버튼 절차를 확인합니다.
2. 디스플레이에서 **BLE 연결 시작**을 누릅니다. 새 페어링은 2분 동안 허용합니다.
3. 통합 Orca의 **설정 → General → Quotamon**에서 ‘디스플레이 찾기’를 누릅니다.
4. Quotamon 장치를 선택합니다. Orca와 디스플레이의 6자리 번호가 일치하면 **양쪽에서 승인**합니다. 요청하지 않은 연결은 거절합니다.
5. 표시할 계정을 체크합니다. 이름·요금제·순서를 편집하고 갱신 주기/표시 항목을 선택한 뒤 저장합니다.
6. 연결과 완전한 스냅샷을 수신하면 홈 카드가 표시됩니다. 선택하지 않은 계정의 잔여량·수집 시각은 전송하지 않습니다. 기기의 계정 선택을 위한 별칭/해시 ID 목록은 전송합니다.

Orca에 로그인되지 않은 서비스는 해당 서비스의 기존 Orca 계정 설정에서 연결하세요. Quotamon은 로그인 창이나 OAuth 인증을 복제하지 않습니다. 사용량 원천이 없는 계정은 확인 불가로 표시하며 로그인 성공만으로 잔여 100%를 만들지 않습니다.

## 설정과 복구

설정은 Orca userData의 `quotamon-settings.json`에 저장합니다. 장치 주소, 해시 계정 ID, 사용자 별칭, 표시 옵션만 포함하고 토큰은 저장하지 않습니다. PC와 보드가 같은 revision을 기반으로 변경해야 저장됩니다. 충돌하면 ‘취소 / 다시 읽기’로 최신 설정을 불러옵니다.

‘자동 연결 해제’는 저장한 대상 주소를 지우고 현재 연결을 끝냅니다. Windows 본드 삭제와는 다른 동작입니다. 다른 PC로 옮길 때는 보드의 **BLE 연결 시작 → 페어링 초기화 → 페어링 지우기**를 사용하고 Windows Bluetooth 설정에서도 기존 장치를 제거하세요.

Copilot은 기존 Windows npm CLI 설치의 선택적 SDK quota RPC를 사용합니다. 별도 SDK 설치나 추론 요청을 만들지 않습니다. 설치 형식·버전이 다르거나 quota가 제공되지 않으면 확인 불가로 남습니다. SDK의 `requests`와 CLI의 `AIC`가 비영점 사용에서도 일치하는지 추가 검증이 필요하여 금액/AIC를 자동 단정하지 않습니다.

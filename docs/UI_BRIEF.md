# Codex 구현 지침: Quotamon

목표: LCD-7 Rev1.2의 800×480 화면에서 구독 잔여량을 읽고 터치로 확인. 기본 구조는 **기존 Orca 무수정 + 설치 없는 포터블 트레이 중계 + ESP32 UI/receiver**입니다. 기존 Orca의 CLI/로컬 사용량 캐시를 재사용하고, 구독 인증/로그인을 중계나 기기에 복제하지 않습니다.

`native/ui.cpp`가 시뮬레이터와 보드의 동일한 LVGL 8.4 UI입니다. 이 공유 구현을 수정하고 렌더를 확인하세요. `assets/providers/`, `native/generated/`의 F108pro 자산·숫자 글리프·라이선스와 새 Copilot 6개 상태를 유지하세요.

카드 5개는 3+2 배치, 전체 카드가 터치 대상, 버튼 최소 48 px, 홈은 스크롤 없이 페이지 이동합니다. 상세는 모든 독립 한도를 표시합니다. 퍼센트 글자는 fill 위 흰색/empty 위 어두운색으로 clipping합니다. 한글·Latin 글리프와 긴 이름 말줄임을 확인하세요.

- 여러 pool을 합치거나 평균하지 않습니다.
- unknown, unlimited, 소진 0%, source stale, transport offline을 구분합니다.
- reset이 지났다는 이유로 100%를 만들지 않습니다. BLE heartbeat로 provider timestamp를 갱신하지 않습니다.
- 월간 quota와 세션 AIC는 별개이며 SDK requests를 검증 없이 AIC/통화로 바꾸지 않습니다.
- Orca 기존 CLI 캐시와 owner-local cache refresh를 재사용합니다. provider 인증 파일 탐색·복사·BLE token 전송을 추가하지 않습니다.

포터블 중계의 온보딩은 계정 선택/이름/순서 → 검색 → 양쪽 숫자 승인입니다. 새로운 pairing은 터치로 연 2분 창에서만 허용합니다. 고정 PIN·자동 승인 금지. 로그인은 Orca 기존 계정 UI를 사용하고 설정 revision 충돌을 처리합니다. PC 설정 UI는 localhost 전용이며 다른 사이트의 요청을 거부합니다.

Snapshot 60 s, heartbeat 10 s, missing traffic 35 s 후 수치 삭제가 기본입니다. Orca 캐시 확인은 15 s, 원천 갱신 요청은 15 min입니다. 원천 조회와 BLE 주기를 분리하고 Retry-After를 존중합니다. UI/core 검사, 포터블 패키지/자식 종료, firmware 링크·메모리, 실제 pair/reject/reconnect/sleep 검증을 각각 기록하세요. README 스크린샷은 mock임을 밝히세요. 선택적인 내부 Orca 패치의 전체 앱 빌드는 기본 포터블 검증과 별개입니다.

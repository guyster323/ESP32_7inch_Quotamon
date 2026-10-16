# README 영상과 계정 수별 화면 검증

README의 한국어·중국어 간체·일본어·영어·독일어 영상은 각 언어의 실제 UI와 자막을 담습니다. 한국어 README의 대표 영상은 한국어, 영어 README의 대표 영상은 영어입니다. 두 문서 모두 5개 언어의 MP4·GIF 링크를 제공합니다.

| 구간 | 내용 |
|---|---|
| 0~5초 | Codex 3계정·Cursor·Copilot의 실제 캐릭터 애니메이션 |
| 5~8초 | 카드 터치와 계정 상세 |
| 8~11초 | 설정과 5개 언어 선택 목록 |
| 11~18초 | 예약 사용, 시각 터치, `2330` 숫자 입력, `23:30` 초안 반영 |
| 18~24초 | 저장, 예시 시각의 화면 끄기, 터치 점등 |

화면은 펌웨어와 공유하는 `native/ui.cpp`에서 생성합니다. 영상은 1200×900, 10fps, 24초이며 LCD 영역은 원래 해상도 800×480로 붙입니다. UI는 모의 데이터이며 실제 계정·프로세스·기기에 접근하지 않습니다. 화면 끄기·터치 점등은 시뮬레이션으로 표시합니다. 실제 보드 시험 결과는 [별도 기록](screen-schedule-hardware.json)에 있습니다.

[플레이어 HTML](../assets/readme/index.html)은 로컬 HTTP 서버나 파일에서 열어 언어를 바꿀 수 있습니다. GitHub README에서는 직접 연결된 MP4·GIF를 엽니다. 생성 프레임·파일 해시·언어·모의 여부는 [media.json](../assets/readme/media.json)에 기록합니다. 한글·라틴 자막은 Noto Sans KR, 중국어는 Noto Sans SC, 일본어는 Noto Sans JP를 사용하며 [라이선스](../THIRD_PARTY_NOTICES.md)를 보존합니다.

## 재생성·검증

기존 네이티브 의존성과 Noto Sans KR/SC Medium을 준비합니다. 일본어 자막용 Noto Sans JP는 고정 커밋과 SHA-256으로 검증한 뒤 내려받습니다. `fontTools`가 필요하며 이 저장소의 `.tools/python` 설치도 사용할 수 있습니다.

```powershell
tools\build_native.cmd
python -m pip install Pillow==12.3.0 imageio-ffmpeg==0.6.0 fonttools
python tools/generate_readme_media.py
python tools/verify_readme_media.py
python tools/verify_account_layouts.py
```

미디어 검사는 5개 언어 파일의 해시·GIF 길이·화면 내 캐릭터 픽셀 변화·검은 화면과 점등 복귀를 확인합니다. 브라우저에서는 각 MP4의 재생·프레임 변화·언어 전환·처음부터 재생·430px 배치를 확인합니다. `--language ko`처럼 한 언어만 미리 생성할 수 있으며 전체 매니페스트와 플레이어는 기본 명령으로 다섯 언어를 모두 생성할 때 갱신합니다.

## 선택한 계정 수별 배치

기존 UI가 개수별 배치를 지원하므로 제품 배치는 변경하지 않았습니다. 한 화면에 최대 5개, 전체 최대 20개이며 초과분은 페이지로 이동합니다.

| 선택 개수 | 기존 배치 | 렌더 예시 |
|---|---|---|
| 0개 | 계정 선택 안내 | [화면](../visuals/account-counts/home-0.png) |
| 1개 | 전체 폭 카드 1개 | [화면](../visuals/account-counts/home-1.png) |
| 2개 | 가로 2열 | [화면](../visuals/account-counts/home-2.png) |
| 3개 | 가로 3열 | [화면](../visuals/account-counts/home-3.png) |
| 4개 | 2×2 | [화면](../visuals/account-counts/home-4.png) |
| 5개 | 위 3개·아래 2개 | [화면](../visuals/account-counts/home-5.png) |

0~5개 선택에서 카드 개수·화면 경계·겹침 없음·각 카드의 올바른 상세 이동·복귀·사용량 갱신 시 위젯 유지, 비연속 계정 선택을 검사합니다. 6개와 20개는 마지막 페이지, 선택을 줄였을 때 페이지 범위 복구까지 검사합니다. [자동 검사 결과](../visuals/account-counts/checks.json)는 실제 공유 LVGL과 포인터 드라이버를 사용하며 실물 촬영이 아닙니다.

# ESP32 런타임 실측

대상은 Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP32-S3 240MHz 듀얼 코어, 8MiB PSRAM, 800×480 RGB565입니다. 애니메이션과 BLE 데이터 전송이 실행 중인 보드에서 통계를 수집합니다. 펌웨어 식별자와 개별 표본은 [실측 데이터](runtime-metrics.json)에 기록합니다.

## 다국어·화면 예약 적용 후 최신 실측

[새 펌웨어 표본](runtime-metrics-languages.json)은 총 80.411초·16개이며 설정 조작 구간도 포함합니다. 홈 5캐릭터의 안정 구간 60.266초·12표본은 CPU 평균 **6.23%**, 실제 LVGL 갱신 **7.48fps**, 렌더·flush 평균 23.77ms였습니다. 전체 관측에서 PSRAM 여유는 **651,272바이트(약 636KiB)**로 일정했고 가장 큰 연속 블록은 **622,592바이트(608KiB)**였습니다. 언어·예약 화면을 조작한 구간의 CPU 값은 유휴 화면 성능으로 해석하지 않습니다.

추가 글꼴·번역의 XIP 예약 때문에 앞선 화면 밀림 수정 펌웨어보다 실제 가용 PSRAM은 약 255KiB 줄었습니다. 두 RGB 버퍼·애니메이션을 유지한 채 PSRAM 예산 하한 검사를 통과했고, 사용자도 실물 언어 변경·글자 표시를 확인했습니다. [다국어·예약 구현 및 검증](LANGUAGES_AND_SCREEN_SCHEDULE.md) · [현재 이미지·실물 적용 기록](character-animation-build.json). 아래 수치는 각 앞선 펌웨어의 기록입니다.

## 주기적인 화면 밀림 수정

계측 추가 후 사용자가 약 4초 간격의 하단 깨짐·화면 밀림을 보고했습니다. 기존 5초 주기 CPU 수집의 `uxTaskGetSystemState()`는 이 프로젝트의 ESP-IDF 5.5.3 듀얼 코어 구현에서 커널 임계 구역을 유지하며 모든 태스크의 미사용 스택까지 순회합니다. RGB 전송은 내부 RAM의 10줄 bounce buffer를 약 0.51ms마다 채워야 하므로, 평균 CPU 사용률이 낮아도 긴 인터럽트 지연은 화면을 밀리게 할 수 있습니다. 실제 중단 시간을 측정한 원인 확정은 아니며, 확인된 지연 위험을 제거한 수정입니다.

CPU 수집은 `ulTaskGetIdleRunTimeCounterForCore()` 두 번과 같은 runtime clock의 차분으로 교체했습니다. 화면 잠금 안에서는 프레임 카운터·페이지 정보만 복사하고, CPU·힙 조회·JSON 생성·BLE 전송은 잠금 밖에서 실행합니다. 잠금을 즉시 얻지 못하면 다음 루프에 재시도해 현재 렌더를 기다리게 하지 않습니다. 5초 통계 주기·스키마·애니메이션·LCD 타이밍·버퍼 수는 유지했습니다.

실제 펌웨어의 수집 코드를 사용하는 호스트 회귀 검사는 전체 태스크 조회 재발, 잠금 중 힙 조회·BLE 전송, 바쁜 UI에서 표본 소실, 프레임 카운터 초기화, 32비트 runtime counter 오버플로를 검사합니다. 실행 명령은 `build/native/quotamon_runtime_metrics_test.exe`입니다.

구현 근거: [ESP-IDF 5.5.3 task 목록·스택 조회](https://github.com/espressif/esp-idf/blob/v5.5.3/components/freertos/FreeRTOS-Kernel/tasks.c), [코어별 idle runtime API](https://github.com/espressif/esp-idf/blob/v5.5.3/components/freertos/esp_additions/freertos_tasks_c_additions.h), [듀얼 코어 커널 임계 구역](https://github.com/espressif/esp-idf/blob/v5.5.3/components/freertos/esp_additions/include/esp_private/freertos_idf_additions_priv.h).

### 수정 후 실물 검증

앱 영역만 업로드한 뒤 새 이미지의 플래시 검증, 이전 앱 백업 해시 일치, 부트로더·파티션·NVS의 업로드 전후 바이트 일치를 확인했습니다. 기존 PC BLE 도우미를 재시작해 인증 연결과 실물 통계 수신을 복구했습니다. 사용자가 **“깨짐·밀림 없이 정상”**이라고 확인했습니다.

홈 캐릭터 5개의 안정 구간 45.260초·9표본을 [수정 후 원시 표본](runtime-metrics-display-fix.json)에 기록했습니다. 초기 페어링 대기 표본 1개와 화면 전환이 섞인 구간 1개는 제외했습니다.

| 항목 | 수정 후 홈 |
|---|---:|
| CPU 두 코어 평균 | 5.97% |
| 코어 0 / 코어 1 평균 | 11.87% / 0.07% |
| CPU 5초 구간 평균 범위 | 5.22~8.12% |
| 실제 LVGL 갱신률 | 7.29fps |
| 평균 / 최대 렌더·flush | 23.03ms / 191ms |
| 현재 / 부팅 이후 최저 PSRAM 여유 | 912,772 / 912,772바이트 |
| 가장 큰 연속 PSRAM 블록 | 884,736바이트 (864KiB) |
| 현재 / 부팅 이후 최저 내부 RAM 여유 | 103,619 / 82,735바이트 |

바이너리는 6,032,080바이트, 프로그램은 6,031,679 / 8,323,072바이트(72.5%), 정적 RAM은 54,160 / 327,680바이트(16.5%)입니다. SHA-256은 `fd9c5d4aee88d3377f45ae0214c3e4fe139088b6766beb9ad18e81fc48f443bf`이며 [현재 배포 기록](character-animation-build.json)과 일치합니다. 관측 중 재부팅이나 PSRAM 여유 감소는 없었습니다. 화면 안정성은 사용자 관찰로 확인한 결과이며 장시간 스트레스 검증은 아닙니다. 앞선 표본과의 차이만으로 성능 향상률을 계산하지 않습니다.

## 2026-10-16 수정 전 실측 결과

실제 Orca·포터블 중계·BLE 연결 상태에서 측정했습니다. 사용자가 연 상세 화면의 첫 전환 표본은 제외했습니다.

| 항목 | 홈: 캐릭터 5개 | 상세: 캐릭터 1개, 2배 |
|---|---:|---:|
| 구간 / 표본 | 100.536초 / 20개 | 80.484초 / 16개 |
| CPU 전체 평균 | 6.10% | 6.08% |
| 코어 0 / 코어 1 평균 | 12.13% / 0.08% | 12.06% / 0.11% |
| CPU 5초 구간 평균 범위 | 5.04~8.47% | 4.66~9.21% |
| 실제 LVGL 갱신률 | 7.43fps | 2.57fps |
| 갱신률의 5초 구간 범위 | 6.78~8.57fps | 1.79~3.16fps |
| 평균 렌더·flush 시간 | 22.87ms | 43.38ms |
| 관측한 최대 렌더·flush 시간 | 190ms | 161ms |
| 현재 가용 PSRAM | 912,804바이트 (891.41KiB) | 912,804바이트 (891.41KiB) |
| 부팅 이후 PSRAM 최저 여유 | 912,804바이트 | 912,804바이트 |
| 가장 큰 연속 PSRAM 블록 | 884,736바이트 (864KiB) | 884,736바이트 (864KiB) |
| 내부 RAM 현재 여유 범위 | 101,483~101,507바이트 | 114,263~114,303바이트 |
| 내부 RAM 부팅 이후 최저 여유 | 80,835바이트 | 80,835바이트 |

상세는 캐릭터가 1개여서 서로 다른 시점에 움직이는 홈의 5개 캐릭터보다 갱신 횟수가 적습니다. 실제 내용 갱신 빈도이며 최대 처리 가능한 FPS를 뜻하지 않습니다. 관측한 최대 렌더 시간에는 전체 갱신·페이지 재구성 비용도 포함됩니다. CPU 범위는 5초 구간 평균의 범위이며 순간 CPU 피크는 아닙니다. 두 구간에서 PSRAM 여유 감소는 없었지만 장시간 누수 검증을 대신하지 않습니다.

계측 펌웨어는 앱 파티션 6,032,263 / 8,323,072바이트(72.5%), 정적 RAM 55,888 / 327,680바이트(17.1%)입니다. 바이너리 6,032,672바이트의 SHA-256은 `d5fec1ffb3d416064951f1fcfe80c9cb1ae6ed2fb1e19b4181a894ee7c7048ea`입니다. 기존 앱 백업, 새 앱 플래시 검증과 부트로더·파티션·NVS 보존 검사를 통과했습니다.

## 측정 방법

| 항목 | 정의 |
|---|---|
| CPU 코어별 사용률 | 5초 구간의 `100 × (1 − idle task 실행시간 증가량 / ESP timer 증가량)` |
| CPU 평균 | 두 코어 사용률의 산술평균. 두 코어 모두 포화일 때 100% |
| LVGL 갱신 FPS | `monitor_cb`에서 센 완료된 갱신 횟수 / 실제 경과 시간 |
| 렌더 시간 | LVGL 갱신·flush 시간의 프레임 가중 평균과 최대값 |
| PSRAM 현재 여유 | `heap_caps_get_info(MALLOC_CAP_SPIRAM).total_free_bytes` |
| PSRAM 최저 여유 | 같은 API의 `minimum_free_bytes`. 부팅 이후 각 힙의 최저 여유 합계 |
| 최대 연속 여유 | `largest_free_block`. 현재 한 번에 할당 가능한 가장 큰 블록 |

CPU는 FreeRTOS task runtime accounting을 켜고 idle task 카운터를 구간 차분한 값입니다. 인터럽트 시간을 별도로 분리한 프로파일러 결과는 아닙니다. CPU 전체 평균과 FPS는 표본의 실제 시간으로 가중하고, 렌더 시간은 프레임 수로 가중합니다. FPS는 변경된 내용이 실제로 그려진 빈도이며, LCD의 물리적 스캔 주파수나 최대 성능 벤치마크가 아닙니다. 정적인 화면에서는 갱신이 적어지는 것이 정상입니다.

PSRAM 값에는 XIP 예약·LCD framebuffer 두 개·LVGL·BLE가 실행 중인 상태의 할당 결과가 반영됩니다. 플래시 사용량이나 빌드 시 정적 RAM 수치와는 구분합니다.

근거: [ESP-IDF FreeRTOS runtime stats](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/freertos_idf.html), [ESP-IDF heap statistics](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/heap_debug.html), [LVGL 8.4 monitor callback](https://lvgl.io/docs/open/8.4/porting/display.html).

## 다시 측정하기

1. `CONFIG_QUOTAMON_RUNTIME_METRICS=y`인 펌웨어와 최신 포터블 중계를 사용합니다. 이 옵션은 FreeRTOS 실행시간 계측을 켭니다.
2. 기존 페어링으로 BLE를 연결합니다. 보드는 5초마다 기존 알림 경로로 통계를 보냅니다.
3. PC 중계는 검증한 수치만 `%APPDATA%\Quotamon\runtime-metrics.json`에 덮어씁니다. 이 통계는 웹 화면의 정기 갱신을 유발하지 않습니다.
4. 원하는 LCD 화면을 유지하고 실행합니다.

```powershell
python tools/collect_runtime_metrics.py --seconds 90 --out docs/runtime-metrics.json
```

수집기는 새로 도착한 표본만 사용하며, 부팅 후 15초 미만·CPU 기준값 미준비·BLE 미연결 표본과 화면 전환이 섞인 첫 구간은 제외합니다. 코어별 CPU, 갱신 FPS, 현재/최저/연속 PSRAM, 내부 RAM과 원시 표본을 JSON으로 저장합니다. 계정 이름·사용량·인증정보·BLE 주소는 저장하지 않습니다.

계측용 FreeRTOS accounting, 5초마다의 샘플링과 BLE 알림에는 작은 비용이 있습니다. 측정 결과는 계측이 켜진 펌웨어의 값이며, 계측을 끈 빌드와 동일하다고 가정하지 않습니다. 장시간 누수·최악 조건의 포화 부하·터치 스트레스 시험은 별도입니다.

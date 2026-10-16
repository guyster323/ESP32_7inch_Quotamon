# ESP32 런타임 실측

대상은 Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP32-S3 240MHz 듀얼 코어, 8MiB PSRAM, 800×480 RGB565입니다. 애니메이션과 BLE 데이터 전송이 실행 중인 보드에서 통계를 수집합니다. 펌웨어 식별자와 개별 표본은 [실측 데이터](runtime-metrics.json)에 기록합니다.

## 2026-10-16 실측 결과

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

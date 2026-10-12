# merge_ec11_led

EC11 로터리 엔코더 드라이버(`ec-11_driver`)와 LED 바 드라이버(`ledbar`)를 한 앱(`fan_ctrl`)에서 같이 쓰는 통합 폴더.

## 구성

| 파일 | 출처 | 비고 |
|---|---|---|
| `ec11.c`, `ec11_ioctl.h`, `ec11_test.c`, `dt.txt` | `ec-11_driver` | **변경 없음** |
| `fan_state.c`, `fan_state.h` | `ec-11_driver` | 팀 공용 `fan_globals.h` 사용, `TEMP`를 `float`로 변경 |
| `fan_ctrl.c` | `ec-11_driver` | LED 바 호출 추가 (`-B` 옵션으로 끌 수 있음) |
| `ledbar.c`, `ledbar_ioctl.h`, `ledbar_ctrl.c/.h`, `fan_globals.h`, `ledbar_test.c`, `fan_led_sim.c`, `ledbar-overlay.dts` | `ledbar` | **변경 없음** |
| `LEDBAR_README.md` | `ledbar/README.md` | LED 바 배선, 핀 설정 |

## 동작

| 모드 | LED 바 | 모드 LED (11번) |
|---|---|---|
| WAIT | 전부 꺼짐 | 꺼짐 |
| AUTO | 온도 기준 칸 수 (`ledbar_ctrl` 규칙) | 켜짐 |
| MANUAL | `PWM_DUTY`를 8칸으로 표시 (0% = 0칸, 1~12% = 1칸 ... 88~100% = 8칸) | 꺼짐 |

`FAN_LEVEL = ceil(PWM_DUTY × 8 / 100)` 로 매 루프마다 갱신하고, `ledbar_ctrl_update()`가 MANUAL일 때 이 값을 표시한다.

노브 1칸 = 5% (`-s`로 변경 가능). 5% 단위일 때의 표시:

| duty | 0 | 5, 10 | 15~25 | 30, 35 | 40~50 | 55, 60 | 65~75 | 80, 85 | 90~100 |
|---|---|---|---|---|---|---|---|---|---|
| LED 바 | 0칸 | 1칸 | 2칸 | 3칸 | 4칸 | 5칸 | 6칸 | 7칸 | 8칸 |

## 빌드와 실행

```bash
make                                   # ec11.ko, ledbar.ko, ec11_test, fan_ctrl
sudo insmod ledbar.ko nodt=1
sudo insmod ec11.ko                    # 노브를 만지지 않은 상태에서
sudo ./fan_ctrl                        # 기본: duty 5% 단위
sudo ./fan_ctrl -B                     # LED 바 없이
```

## 핀 (겹치지 않음)

| 장치 | 헤더 핀 |
|---|---|
| EC11 S1, S2, KEY | 29, 31, 35 |
| 모드 LED | 11 |
| LED 바 1~8 | 12, 13, 16, 18, 22, 37, 38, 40 |

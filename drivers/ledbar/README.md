# LED 바 드라이버 (선풍기 프로젝트)

빌드 경로 변경: `make -C drivers/ledbar`는 커널 모듈만, `make -C Testbench`는 테스트 앱을 빌드합니다. 아래 명령은 저장소 루트에서 실행하는 기준입니다.

Jetson Orin Nano (JetPack 6 / L4T R36.5, Linux 5.15)용 8칸 LED 바 커널 드라이버와
팀 전역변수 기반 표시 제어 코드.

| 파일 | 내용 |
|---|---|
| `ledbar.c` | 커널 모듈 (platform driver + misc device + 커널 타이머 애니메이션) |
| `ledbar_ioctl.h` | 커널/앱 공용 헤더 |
| `ledbar_ctrl.c` / `ledbar_ctrl.h` | 전역변수(`fan_mode`, `TEMP`, `FAN_LEVEL`) 보고 LED 바 갱신 → **메인 앱에 같이 빌드** |
| `Application/fan_globals.h` | 팀 공용 전역변수 선언 |
| `Testbench/ledbar_test.c` | 드라이버 단독 테스트/시연 앱 |
| `Testbench/fan_led_sim.c` | 팀원 코드 없이 `ledbar_ctrl` 동작 확인하는 시뮬레이터 |
| `ledbar-overlay.dts` | DT 오버레이 (⚠️ 아직 실기 적용 확인 안 됨, 아래 참고) |
| `Makefile` | `make` = 커널 모듈, `make dtbo` = 오버레이; 테스트/시뮬레이션은 Testbench에서 빌드 |

## 1. 배선

```
젯슨 헤더 핀 ──→ LED 바 애노드(+) ──LED── 캐소드(−) ──→ 9A331G 2~9번 핀
                                                       9A331G 1번 핀(● 점) ──→ GND
```

| LED | 위치 | 헤더 핀 | GPIO | gpiochip0 라인 |
|---|---|---|---|---|
| LED1 | 맨 아래 | 12 | PH.07 | 50 |
| LED2 | | 13 | PY.00 | 122 |
| LED3 | | 16 | PY.04 | 126 |
| LED4 | | 18 | PY.03 | 125 |
| LED5 | | 22 | PY.01 | 123 |
| LED6 | | 37 | PY.02 | 124 |
| LED7 | | 38 | PI.01 | 52 |
| LED8 | 맨 위 | 40 | PI.00 | 51 |
| 저항 공통 | | GND | | |

## 2. 핀 설정 (JetPack 6 필수)

JetPack 6 에서는 헤더 핀이 `tristate=1`(출력 꺼짐)로 부팅되어 GPIO에 값을 써도 LED가 안 켜진다.

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
# 위 8개 핀을 GPIO로 설정 → Save and reboot to reconfigure pins
```

확인 (`tristate=0` 이면 OK):

```bash
sudo cat /sys/kernel/debug/pinctrl/2430000.pinmux/pinconf-groups | grep -A4 -E "soc_gpio41_ph7|spi3_sck_py0"
```

## 3. 빌드 / 로드

```bash
sudo apt install build-essential device-tree-compiler gpiod nvidia-l4t-kernel-headers
make -C drivers/ledbar
sudo insmod drivers/ledbar/ledbar.ko nodt=1
sudo dmesg | tail          # "LED 바 8칸 준비 완료 (/dev/ledbar)"
sudo rmmod ledbar          # 내리면 전부 소등
```

`nodt=1` 은 DT 오버레이 없이 gpiochip 라인 번호(`lines=` 기본값 위 표)로 바로 GPIO를 받는다.

## 4. 드라이버 테스트

```bash
echo 5 > /dev/ledbar          # 아래부터 5칸
echo "p 0xA5" > /dev/ledbar   # 비트마스크
echo "blink 300" > /dev/ledbar
echo wave > /dev/ledbar       # 1→8→0, 끝나면 원래 표시로
echo progress > /dev/ledbar   # 3초 동안 차오름
echo stop > /dev/ledbar
cat /dev/ledbar               # 상태
cat /sys/devices/platform/ledbar/level

make -C Testbench ledbar_test && ./Testbench/ledbar_test demo
```

## 5. 메인 앱 연동 (`ledbar_ctrl`)

```c
#include "fan_globals.h"
#include "ledbar_ctrl.h"

ledbar_ctrl_init();
while (running) {
    /* 다른 팀원 코드가 TEMP, FAN_LEVEL, fan_mode 갱신 */
    ledbar_ctrl_update();     /* 칸 수가 바뀔 때만 ioctl */
}
ledbar_ctrl_close();
```

빌드 시 `ledbar_ctrl.c` 포함, `-lm` 필요. 전역변수 정의(`float TEMP; int FAN_LEVEL = 1; ...`)는 메인 앱 한 곳에서.

| 모드 | LED 바 |
|---|---|
| `WAIT` | 전부 꺼짐 |
| `AUTO` | 21.0°C 미만 0칸, 21.x 1칸 … 27.x 7칸, 28°C 이상 8칸 (경계 ±0.2°C 히스테리시스) |
| `MANUAL` | `FAN_LEVEL` (1~8) 그대로 — 단수 계산은 엔코더/모터 담당 |
| `WAIT` → 켜짐 | 웨이브 효과 한 번 |

동작 확인:

```bash
make -C Testbench sim && ./Testbench/fan_led_sim
```

## 6. DT 오버레이 (선택, 미검증)

`ledbar-overlay.dts` 에는 `team,ledbar` 노드와, probe 시 tristate/open-drain 을 해제하는 pinctrl 설정이 있다.
`/boot/extlinux/extlinux.conf` 에 `OVERLAYS /boot/ledbar.dtbo` 를 넣어봤지만 `/proc/device-tree/ledbar` 가 생기지 않았다
(원인 미확인 — compatible 불일치 가능성). 현재는 **jetson-io + `nodt=1`** 로 동작한다.

## 7. 문제 해결

| 증상 | 확인 |
|---|---|
| `insmod` 시 `Invalid module format` | 커널 헤더 버전과 `uname -r` 일치 확인 |
| GPIO 쓰는데 LED가 안 켜짐 | 2번 핀 설정(tristate) 확인 |
| `gpioset` 에서 `Device or resource busy` | `ledbar` 모듈이 핀을 잡고 있음 → `sudo rmmod ledbar` |
| `/dev/ledbar` 열기 실패 | `lsmod | grep ledbar` 로 모듈 로드 확인 |
| LED가 너무 어두움 | 헤더 구동 전류 한계 → 트랜지스터 어레이(ULN2803 등) 고려 |

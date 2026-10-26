# 커널 드라이버 기반 실시간 온도 감시·냉각 제어

Jetson Orin Nano에서 센서·구동부 4종을 **리눅스 커널 드라이버**로 직접 작성하고, 사용자 공간 앱에서 온도에 따라 팬 속도를 자동 제어하는 프로젝트입니다. (팀 프로젝트)

`C` · `Linux Kernel Module` · `Device Tree Overlay` · `I2C / IIO` · `PWM` · `GPIO IRQ` · `Jetson Orin Nano`

## 구성

| 드라이버 | 인터페이스 | 내용 |
| --- | --- | --- |
| **BMP180** | I2C → IIO | 보정 계수를 읽어 온도를 계산하고 `in_temp_input` sysfs로 노출 |
| **DC Motor** | PWM + GPIO | DT의 `pwms` / `in1·in2-gpios`로 방향·속도 제어, `ioctl`로 듀티 설정 |
| **EC11 Encoder** | GPIO 인터럽트 | 회전·클릭·롱프레스 판정, 디바운스 타이머, `kfifo` + `poll` 이벤트 전달 |
| **LED Bar** | GPIO 배열 | 단계 표시와 커널 타이머 애니메이션, `ioctl` / sysfs 제어 |

## 동작

```
BMP180 (IIO) ──온도──▶            ┌─ AUTO   : 22℃ 이상 ON / 20℃ 미만 OFF (히스테리시스)
                       fan_app ───┤           22~32℃ 구간 듀티 40~100% 선형 증가
EC11 (poll)  ──이벤트─▶            ├─ MANUAL : 엔코더 회전으로 듀티 5%씩 조절
                                  └─ WAIT   : 롱프레스로 정지·대기
                         │
                         ├──ioctl──▶ DC Motor (PWM)
                         └──ioctl──▶ LED Bar (팬 단계 0~8)
```

## 폴더 구조

```
drivers/
├── bmp180/      I2C 온도 센서 IIO 드라이버 + DT 오버레이
├── dc_motor/    PWM 모터 드라이버 + DT 오버레이
├── ec11/        로터리 엔코더 인터럽트 드라이버
└── ledbar/      LED 바 드라이버
Application/     fan_app (WAIT / AUTO / MANUAL 모드 제어)
Testbench/       드라이버별 단위 테스트
```

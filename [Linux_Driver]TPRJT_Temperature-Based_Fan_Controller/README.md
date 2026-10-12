# PWM 팬(모터) 제어 커널 드라이버

Jetson Orin Nano에서 GPIO와 PWM으로 팬 또는 모터를 구동하기 위한 Linux 문자 디바이스 드라이버입니다. 사용자 프로그램 `apptest`가 `/dev/mydev`에 `ioctl` 요청을 보내면 커널 모듈 `devtest`가 PWM 듀티를 설정합니다.

> 디렉터리 이름과 달리 **현재 코드에는 온도 측정이나 온도에 따른 자동 속도 조절 기능이 없습니다.** 듀티(0~100%)를 사용자가 직접 지정합니다.

## 구성

| 파일 | 역할 |
| --- | --- |
| `devtest.c` | 문자 디바이스 등록, GPIO 방향 설정, PWM 제어 및 `ioctl` 처리 |
| `apptest.c` | 듀티를 입력받아 `/dev/mydev`에 `ioctl` 요청을 보내는 사용자 프로그램 |
| `my_ioctl.h` | 양쪽 프로그램이 공유하는 `MY_IOCTL_CMD_SET_PWM` 명령 정의 |
| `Makefile` | 커널 모듈 `devtest.ko`와 사용자 프로그램 `apptest` 빌드 |

드라이버에 지정된 값은 다음과 같습니다. 실제 보드의 핀 배치와 PWM 설정이 이 값에 맞는지 확인해야 합니다.

| 항목 | 코드의 값 |
| --- | --- |
| 디바이스 노드 | `/dev/mydev` |
| 문자 디바이스 번호 | major `120`, minor `0` |
| 모터 방향 GPIO | `GPIO_IN1=492`, `GPIO_IN2=433` |
| PWM 채널 | `PWM_ID=2` |
| PWM 주기 | `1,000,000 ns` (1 kHz) |

듀티가 1~100%이면 IN1을 HIGH, IN2를 LOW로 설정하고 PWM을 활성화합니다. 듀티가 0%이거나 디바이스를 닫으면 두 GPIO를 LOW로 만들고 PWM을 비활성화합니다. 지원하지 않는 `ioctl` 명령은 `ENOTTY`, 범위를 벗어난 듀티는 `EINVAL`을 반환합니다.

## 빌드

실행할 보드의 커널에 맞는 커널 헤더/빌드 디렉터리와 C 컴파일러가 필요합니다. `KERNELDIR` 기본값은 `/lib/modules/$(uname -r)/build`이며, `APP_CC` 기본값은 `aarch64-linux-gnu-gcc`입니다.

보드에서 직접 빌드할 때:

```sh
make APP_CC=gcc
```

다른 컴퓨터에서 빌드할 때는 **실행 대상 보드의 커널 빌드 디렉터리**를 지정하고, 커널 모듈과 사용자 프로그램 모두 대상 아키텍처용으로 빌드합니다. 예를 들어 ARM64 대상이라면:

```sh
make KERNELDIR=/path/to/target/kernel/build ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- APP_CC=aarch64-linux-gnu-gcc
```

## 실행

다음 명령은 대상 보드에서 실행합니다. 먼저 배선과 GPIO/PWM 채널 설정을 확인하고, 빌드한 파일을 보드에 준비합니다.

```sh
sudo insmod devtest.ko
sudo mknod /dev/mydev c 120 0  # 해당 노드가 없을 때만
sudo ./apptest 50              # 듀티 50%
```

`apptest`는 0~100 사이의 정수 하나를 받습니다. 0보다 큰 값을 지정하면 Enter를 누를 때까지 프로그램이 대기합니다. Enter를 누르면 디바이스가 닫히며 드라이버의 `release` 함수가 모터를 정지시킵니다. `sudo ./apptest 0`은 즉시 정지 요청을 보냅니다. 프로그램이 실행되는 동안 다른 프로세스가 같은 장치를 제어하면 PWM 값이 바뀔 수 있습니다.

사용을 마친 뒤:

```sh
sudo rmmod devtest
sudo rm /dev/mydev  # 위에서 수동으로 만든 노드라면 제거
```

빌드 결과물을 지우려면 `make clean`을 실행합니다.

## 참고 사항

- `insmod`가 실패하면 `dmesg`에서 오류를 확인하세요. major 번호 충돌, GPIO 사용 중, PWM 채널 사용 불가, 대상 커널과 모듈의 빌드 버전 차이 등이 원인일 수 있습니다.
- `apptest`가 `/dev/mydev`를 열 수 없다면 모듈 로드 여부, 디바이스 노드의 번호와 권한을 확인하세요.
- 이 코드는 고정 GPIO 번호와 기존 GPIO/PWM 커널 API를 사용하므로, 다른 보드나 커널 버전에서는 핀 번호 또는 드라이버 코드를 조정해야 할 수 있습니다.

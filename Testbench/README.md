# Driver Testbench

Prior integrated-folder documentation is preserved as `README_legacy.md`.

Unit and integration test applications moved out of `drivers/`.

| Test | Purpose |
|---|---|
| `ec11_test.c` | EC11 event/read/ioctl test |
| `ledbar_test.c` | LED Bar driver standalone test |
| `fan_ctrl.c` + `fan_state.c/.h` | EC11 + LED Bar integration test; motor PWM callback is a TODO |
| `fan_led_sim.c` | Simulated fan states and LED Bar display |

From the repository root:

```bash
make -C drivers/ec11
make -C drivers/ledbar
make -C Testbench
make -C Testbench sim
sudo insmod drivers/ledbar/ledbar.ko nodt=1
sudo insmod drivers/ec11/ec11.ko long_press_ms=3000
sudo ./Testbench/fan_ctrl
```

`fan_ctrl -B` disables LED Bar, `-r` toggles rotary direction inversion.
Do **not** run `fan_ctrl` and `Application/fan_app` at the same time: both
consume events from the same `/dev/ec11` queue.

Display rules: WAIT=0 bars; AUTO=temperature based; MANUAL uses
`FAN_LEVEL = ceil(PWM_DUTY * 8 / 100)`.
The shared state declarations live in `Application/fan_globals.h`.

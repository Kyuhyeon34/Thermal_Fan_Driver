# EC11 Rotary Encoder Driver

The kernel module `ec11.ko` exposes the character device `/dev/ec11`.
The shared user/kernel ABI is declared in `ec11_ioctl.h`.

From repository root:

```bash
make -C drivers/ec11
sudo insmod drivers/ec11/ec11.ko long_press_ms=3000
make -C Testbench ec11_test
sudo ./Testbench/ec11_test
```

EC11 GPIO header pins: S1=29, S2=31, KEY=35. The default `long_press_ms` in
`ec11.c` is 2000; the fan project requires 3000 milliseconds. Load the driver
while the knob is at rest. `dt.txt` contains the original pin/device-tree notes.

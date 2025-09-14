# NUCLEO-F767ZI + Zephyr (LED + Ethernet)

Board docs: https://docs.zephyrproject.org/latest/boards/st/nucleo_f767zi/doc/index.html

## Quick Setup
Activate virtual environment:
```bash
source ~/zephyrproject/.venv/bin/activate
```

Build & flash:
```bash
west build -b nucleo_f767zi K2-Zephyr -d build/app
west flash -d build/app
```

Optionally on WSL
```
west flash -d build/app --runner openocd
```

Monitor serial (115200 baud):
```bash
minicom -D /dev/ttyACM0 -b 115200
```
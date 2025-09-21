# NUCLEO-F767ZI + Zephyr

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

Optionally on WSL or if STM32CubeProgrammer not installed:
```
west flash -d build/app --runner openocd
```

Monitor serial (115200 baud):
```bash
minicom -D /dev/ttyACM0 -b 115200
```


## vscode config

in .vscode folder add this and customize to your need

```{
    "configurations": [
    {
        "name": "Zephyr",
        "includePath": [
            "${workspaceFolder}/**",
            "/home/YOUR_USERNAME/zephyrproject/zephyr/include/**",
            "/home/YOUR_USERNAME/zephyrproject/zephyr/lib/libc/newlib/include/**",
            "${workspaceFolder}/build/zephyr/include/generated/**",
            "${workspaceFolder}/build/zephyr/include/**",
            "/home/YOUR_USERNAME/zephyr-sdk-VERSION/arm-zephyr-eabi/arm-zephyr-eabi/include/**"
        ],
        "defines": [
            "CONFIG_BOARD=\"nucleo_f767zi\"",
            "__ZEPHYR__=1"
        ],
        "compilerPath": "/home/YOUR_USERNAME/zephyr-sdk-VERSION/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc",
        "cStandard": "c11",
        "cppStandard": "c++17",
        "intelliSenseMode": "gcc-arm"
    }
],
"version": 4
}```
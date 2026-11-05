# 电感分拣（8051 + LCD1602 + 舵机 + Proteus）

AT89C51 读取 LC 振荡/比较器输出的方波频率，换算成电感量，用 LCD1602 显示，并驱动舵机把合格/不合格件分到不同位置。仓库包含 Proteus 工程、Keil C51 工程、SDCC 源码和预编译 HEX。

## 功能

- 默认目标：`1.00 mH`，容差 `±5%`
- 1 秒闸门计频，5 次中值滤波
- 合格：舵机转到 90°；不合格：舵机回到 0°
- 短按 `+/-` 每次 0.1 mH；长按 1 秒后每 0.2 秒连发 0.5 mH
- 双键同时按住 2 秒：用标准件重新校准
- LCD1602 显示电感值、目标值、合格/不合格

## 接线

| 信号 | 引脚 | 说明 |
| --- | --- | --- |
| LCD1602 D0-D7 | P0 | 需要排阻上拉 |
| LCD RS / RW / E | P2.0 / P2.1 / P2.2 | |
| 频率输入 | P3.5 / T1 | LC 振荡或比较器输出方波 |
| 舵机 PWM | P1.0 | 1.0 ms=0°，1.5 ms=90°，50 Hz |
| 按键 + / - | P1.1 / P1.2 | 低电平有效 |
| 晶振 | 12 MHz | AT89C51 |

## 目录

| 路径 | 内容 |
| --- | --- |
| `proteus/inductance-sorter.pdsprj` | Proteus 仿真工程 |
| `firmware/keil/main.c` | Keil C51 源码 |
| `firmware/keil/111.uvproj` | Keil 工程 |
| `firmware/sdcc/main_sdcc.c` | SDCC 版本源码 |
| `firmware/sdcc/Makefile` | SDCC 构建脚本 |
| `firmware/prebuilt/exp6_8051.hex` | 预编译 HEX |

## 快速开始（Proteus）

1. 用 Proteus 8.13+ 打开 `proteus/inductance-sorter.pdsprj`。
2. 双击 AT89C51，**Program File** 选择 `firmware/prebuilt/exp6_8051.hex`，**Clock Frequency** 设为 `12` MHz。
3. 运行仿真，按 `+/-` 调整目标值，观察 LCD 和舵机。

> 如果 `.pdsprj` 里的固件路径仍指向旧目录，重新选择一次 HEX 即可，不影响原理图。

## 构建

### Keil C51

打开 `firmware/keil/111.uvproj`，确认器件为 AT89C51，编译生成 HEX。

### SDCC

```bash
cd firmware/sdcc
make
```

生成 `main_sdcc.ihx`，再用 `packihx` 转成 HEX：

```bash
packihx main_sdcc.ihx > exp6_8051.hex
```

## 许可证

原创代码与文档采用 MIT。`STARTUP.A51` 是 Keil 的标准启动文件，版权归 Keil/ARM，保留原样仅用于工程完整性。见 [NOTICE.md](NOTICE.md)。

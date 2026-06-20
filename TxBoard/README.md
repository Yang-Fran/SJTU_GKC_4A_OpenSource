# TxBoard 发射机固件

`TxBoard/` 是发射端 TM4C1294 控制固件。程序生成测试序列，维护调制状态机，通过串行接口驱动 AD9850 DDS 模块，并使用 TM1638 数码管/按键模块进行模式显示和切换。

> 本目录继承 Keil/Windows 中文工程习惯，部分源码注释使用 GB2312/GBK 编码。修改 `.c/.h` 文件时请使用 GBK、GB2312 或系统默认 ANSI 打开和保存，不要让编辑器自动转换为 UTF-8。

## 文件结构

```text
TxBoard/
|-- dds_ser_main.c         主状态机、序列生成和调制逻辑
|-- ad9850_serial.c/.h     AD9850 串行模式驱动
|-- tm1638.c/.h            TM1638 显示和按键驱动
|-- dds-ser.uvprojx        Keil uVision 工程文件
|-- dds-ser.uvoptx         Keil 工程选项
|-- RTE/                   Keil RTE 和设备启动文件
|-- inc/                   TM4C1294 设备头文件
`-- driverlib/             TI TivaWare DriverLib 源码和预编译库
```

## 调制模式

主状态机支持以下输出：

- Carrier：固定 100 kHz 正弦载波。
- ASK：按序列控制 100 kHz 载波通断。
- FSK：在 95 kHz 和 105 kHz 两个频率之间切换。
- PSK：100 kHz 载波的 0/180 度相位切换。
- DPSK：使用差分序列控制的相移键控。
- QPSK：固定 8 bit 序列按两比特分组，映射到四个相位状态。

TM1638 按键用于切换工作状态，LED 指示当前状态。`PE1/PE2/PE3` 用作序列或调试输出。

## AD9850 接口

`ad9850_serial.c` 将 AD9850 置于串行模式并写入 40 bit 控制字：

- `RESET`：`PE5`
- `W_CLK`：`PC6`
- `FQ_UD`：`PC5`
- `D7`：`PC4`
- 参考时钟：125 MHz

`AD9850_WriteCmd()` 根据目标频率计算 32 位频率控制字 FTW，并附加相位/控制字节。

## 导入和构建

使用 Keil uVision 打开 `dds-ser.uvprojx`即可。

`Objects/`、`Listings/`、`*.map`、`*.dep`、`*.uvguix.*` 等是构建或用户界面状态文件，删除不影响工程导入和重新构建。`driverlib/` 中的 TI 文件保留其原许可。

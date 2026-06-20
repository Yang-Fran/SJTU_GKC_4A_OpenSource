# 硬件接口和接线概要

本项目由多个固件控制板和模拟电路板共同组成。仓库不包含完整 PCB 源文件，本说明只记录固件侧可以确定的数字接口和联调顺序。

## 系统信号流

```text
TxBoard DDS 输出
  -> 发射机输出调理
  -> RLC/信道电路
  -> 接收前端滤波和 AGC
  -> Costas I/Q 解调
  -> 基带调理
  -> TM4C123 基带译码
```

## TxBoard 发射端

- MCU：TM4C1294。
- DDS：AD9850，串行控制模式，125 MHz 参考时钟。
- AD9850 接线：`PE5` reset，`PC6` W_CLK，`PC5` FQ_UD，`PC4` D7。
- 序列/调试输出：`PE1`、`PE2`、`PE3`。
- 人机交互：TM1638 数码管/按键模块。

## AGC 板

- MCU：MSP430G2553 LaunchPad。
- ADC 检波输入：`P1.4/A4`、`P1.5/A5`。
- DAC8043 控制接口：`P2.0` 时钟、`P2.1` 串行数据、`P2.2` 装载。
- UART：`P1.1/RXD`、`P1.2/TXD`，9600 baud。
- 按键：`P1.3`。
- LED：`P1.0`、`P1.6`。

## Costas 控制器

- MCU：Nuvoton NUC140/NUC1xx 系列。
- 板级驱动控制双路 AD9850，产生正交本振。
- Costas 程序注释中记录 I/Q 采样：I 对应 ADC5/GPA5，Q 对应 ADC6/GPA6。
- `T POINT CAL` 用于测量 I/Q 两路 ADC 的直流中心和幅度。
- `FREQ CAL` 需要外部准确 100 kHz 参考信号，用于估计 AD9850 实际参考时钟。

## baseband 基带板

- MCU：TM4C123GH6PM。
- ADC 输入：`PE1/AIN2`。
- 恢复位时钟输出：`PE2`。
- 判决输出：`PA2`。
- 差分译码输出：`PA3`。
- DAC7611 观测接口：`PD0` CS，`PD1` CLK，`PD2` SDI，`PD3` LD。
- ISR 时间探针：`PF1`。

## 推荐联调顺序

1. 在 TxBoard 上先验证 100 kHz carrier 模式，再验证各调制模式。
2. 在 AGC 上通过 UART 查看 ADC 均值和 DAC 控制量是否正常。
3. 在 Costas 上先做 T 点标定，再做频率校准，最后进入 BPSK/QPSK 跟踪。
4. 在 baseband 上观察恢复位时钟、判决输出和差分译码输出是否稳定。

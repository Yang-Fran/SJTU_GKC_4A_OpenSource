# AGC 自动增益控制固件

`AGC/` 是接收机前端自动增益控制板的 MSP430G2553 固件。程序周期采样两路检波信号，做滑动平均/定时更新，并通过 DAC8043 风格的 12 位串行 DAC 输出控制量，用于调节模拟增益链路。

## 文件结构

```text
AGC/
|-- main.c                 主程序、ADC/UART/Timer/DAC 控制逻辑
|-- lnk_msp430g2553.cmd    TI MSP430G2553 链接脚本
|-- targetConfigs/         CCS 目标配置
|-- .ccsproject            Code Composer Studio 工程元数据
|-- .cproject              Eclipse CDT 工程元数据
|-- .project               Eclipse 工程元数据
`-- .clangd                C 语言服务配置
```

## 核心逻辑

- `Timer0_A0` 以 5 ms 为节拍运行。
- ADC10 采样 `P1.4/A4` 和 `P1.5/A5` 两路检波输入。
- 每 10 次采样更新一次平均值，降低瞬时噪声和纹波影响。
- `P1.3` 按键切换控制状态。
- `DAC8043()` 使用 `P2.0/P2.1/P2.2` 位操作写入 12 位控制字。
- UART 配置为 9600 baud、8 位数据、无校验，用于调试输出。

## 硬件接口

- MCU：MSP430G2553 LaunchPad。
- ADC 输入：`P1.4/A4`、`P1.5/A5`。
- DAC 串行接口：`P2.0` 时钟、`P2.1` 数据、`P2.2` 装载。
- 按键：`P1.3`。
- 指示灯：`P1.0`、`P1.6`。
- UART：`P1.1/RXD`、`P1.2/TXD`，需要确认 LaunchPad RXD/TXD 跳线帽处于 UART 模式。

## 导入和构建

使用 TI Code Composer Studio 导入本目录工程，目标器件选择 MSP430G2553。`Debug/` 是 CCS 自动生成的构建输出，删除后不影响工程导入，会在下一次构建时重新生成。

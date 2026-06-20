# COSTAS 环解调控制固件

`COSTAS/` 是接收机 Costas 环控制器固件，目标平台为 Nuvoton NUC140/NUC1xx 系列。该部分负责控制两路 AD9850 本地正交载波、采集 I/Q 两路 T 点信号、执行 T 点标定、DDS 参考时钟频率校准，以及 BPSK/QPSK 的 bang-bang 相位追踪。

> 项目早期代码注释可能使用 GB2312/GBK 编码。查看或修改源码时请注意 IDE 编码设置，避免无意转换导致中文注释乱码。

## 文件结构

```text
COSTAS/
|-- COSTASProject/      项目层代码：主程序、UI、模式状态机、Costas/T 点/频率校准
|-- BoardSupport/       板级驱动：AD9850、ADC、LCD、按键等
|-- Dependencies/       NUC1xx 厂商驱动和板级支持库
|-- CMSIS/              Cortex-M0/NUC1xx 启动与系统文件
|-- project0904.uvproj  Keil uVision 工程文件
|-- project0904.uvopt   Keil 工程选项
|-- project0904.sct     Keil scatter 文件
`-- CMakeLists.txt      CLion/语言服务器索引用配置
```

`project0904.uvproj` 是权威构建入口；`CMakeLists.txt` 只用于代码索引和跳转，不作为固件下载流程。

## 分层约定

项目代码按依赖方向分为四层：

```text
项目层 COSTASProject
  -> 板层 BoardSupport
  -> 芯片层 Dependencies/Include 与 Dependencies/Src
  -> 启动层 CMSIS
```

上层可以依赖下层；下层不应反向依赖上层。

## 构建注意事项

Keil 工程需要包含：

- `CMSIS/startup_NUC1xx.s`
- `CMSIS/CM0/CoreSupport/core_cm0.c`
- `CMSIS/CM0/DeviceSupport/Nuvoton/NUC1xx/system_NUC1xx.c`
- `Dependencies/Src/Driver/` 下需要的 NUC1xx 驱动源文件
- `Dependencies/Src/NUC1xx-LB_002/` 下需要的板级库源文件
- `BoardSupport/` 下本项目改写或封装的板级驱动
- `COSTASProject/` 下项目层应用代码

相对原始厂家驱动，本工程应使用 `BoardSupport/Scan_6Key.c` 替代旧扫描按键实现，并使用 `BoardSupport/LCD_Driver_Modified.c` 替代原 LCD 驱动。USB 相关驱动未作为本项目核心路径使用。

## 板级驱动

`BoardSupport/AD9850_func.c` 封装 DDS 控制。上层以整数 Hz 或毫 Hz 接口设置频率，驱动根据当前参考时钟估计值换算 FTW。Costas 闭环运行时主要叠加离散相位字，AD9850 5 bit 相位控制字范围为 `0..31`，每档约 `11.25` 度。

`BoardSupport/adc_sample.c` 提供 I/Q ADC 采样接口；`LCD_Driver_Modified` 和 `Scan_6Key` 用于本机人机交互。

## 工作模式

项目层代码负责系统初始化、按键/LCD 交互、运行模式切换和周期任务调度。模式选择分为两页：

- 第一页：双路独立测试、双路相位测试、校准入口。
- 第二页：BPSK Costas、对角 QPSK、轴向三态 QPSK。

校准入口包含：

- `T POINT CAL`：测量 I/Q 原始 ADC 的最小值和最大值，计算直流中心 `MID=(MAX+MIN)/2` 和幅度 `AMP=(MAX-MIN)/2`。
- `FREQ CAL`：使用外部准确 100 kHz 信号和本地约 99.9 kHz 正交本振形成约 100 Hz 拍频，估计 AD9850 实际参考时钟。

## Costas 闭环算法

BPSK 与 QPSK 共用一套复平面相位跟踪框架：

1. 使用 T 点标定结果将 ADC 采样归一化到近似零中心、等幅度的 I/Q 坐标。
2. 根据调制方式选择理想星座点。
3. 将当前点分解为沿理想方向的投影 `A` 和垂直方向误差 `B`。
4. 使用 `phase_error = B/A * 1024` 的定点近似表示小角度相位误差。
5. 对相位误差和相位速度做低通。
6. 当误差超过阈值时，将 AD9850 相位字按 `11.25` 度步进一档。
7. 相位步进后冻结若干采样，避免相位跳变被误判成频偏。
8. 连续满足锁定条件后进入锁定状态。

三种判决模式：

- BPSK：判决到正/负 I 轴标准点。
- 对角 QPSK：判决到 `(+-1,+-1)` 四个对角星座点。
- 轴向 QPSK：判决到 `(+-1,0)/(0,+-1)` 四个坐标轴星座点，并在 `|I|≈|Q|` 的边界附近暂停更新。

当前实现中，BPSK/QPSK 运行期不再持续调节 DDS 频率，而是依赖已校准的频率估计和离散相位追踪。

## 状态指示

程序使用 LED 表示闭环或校准状态：

- 快闪：当前 ADC 点无效或信号幅度不足。
- 慢闪：闭环或校准正在跟踪。
- 常亮：闭环或校准达到连续锁定条件。


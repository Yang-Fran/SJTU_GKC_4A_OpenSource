# 数字调制解调通信系统

本仓库是一个多嵌入式设备协同工作的数字调制/解调通信系统开源仓库。项目整体由发射端、接收前端 AGC、Costas 环解调和基带恢复四个相对独立的固件部分组成；获取本仓库代码的人默认已经了解项目的基本背景，因此 README 重点说明代码结构、工具链、工程导入和开源边界。

## 仓库结构

```text
.
|-- TxBoard/       TM4C1294 发射机固件，控制 AD9850 产生 ASK/FSK/PSK/DPSK/QPSK 等信号
|-- AGC/           MSP430G2553 自动增益控制固件，采样检波输出并控制 DAC8043
|-- COSTAS/        NUC140/NUC1xx Costas 环控制器固件，完成 I/Q 采样、校准和载波相位追踪
|-- baseband/      TM4C123 接收基带固件，完成滤波、位同步、判决和差分译码
|-- docs/          硬件接线、开源准备和工程导入说明
|-- LICENSE        项目自有代码和文档的 MIT License
`-- .gitignore     构建产物、用户态 IDE 文件和本地报告材料忽略规则
```


## 四个子项目

- [TxBoard](TxBoard/README.md)：发射端控制程序。TM4C1294 通过串行接口驱动 AD9850 DDS，并使用 TM1638 模块进行模式显示和按键切换。
- [AGC](AGC/README.md)：接收前端自动增益控制程序。MSP430G2553 周期采样两路检波 ADC，平均后通过 DAC8043 输出增益控制量。
- [COSTAS](COSTAS/README.md)：Costas 环控制程序。NUC140/NUC1xx 控制双路 DDS 正交本振，完成 T 点标定、频率校准、BPSK/QPSK bang-bang 相位追踪。
- [baseband](baseband/README.md)：接收基带程序。TM4C123 对 Costas 环输出进行 ADC 采样、数字滤波、DPLL 位同步、积分判决和差分译码。

## 工具链

本仓库保留了各子项目原始工程文件。具备完整工具链时，通常可以直接导入对应工程：

- `AGC/`：TI Code Composer Studio，目标芯片 MSP430G2553。
- `baseband/`：TI Code Composer Studio，目标芯片 TM4C123GH6PM。
- `TxBoard/`：Keil uVision，工程文件为 `TxBoard/dds-ser.uvprojx`。
- `COSTAS/`：Keil uVision，工程文件为 `COSTAS/project0904.uvproj`；`COSTAS/CMakeLists.txt` 仅用于 CLion/语言服务器索引。

删除 `Debug/`、`objects/`、`Objects/`、`Listings/`、`cmake-build-debug/` 等构建产物不会影响正常工程导入。IDE 会在首次构建时重新生成这些目录。需要注意的是，部分 IDE 可能会因为本机安装路径不同而要求重新选择编译器、调试器或芯片包。

## 文档

- [docs/hardware-wiring.md](docs/hardware-wiring.md)：硬件接口和接线概要。
- [docs/encoding.md](docs/encoding.md)：Keil 工程源码的 GB2312/GBK 编码说明。
- [docs/import-and-build.md](docs/import-and-build.md)：各工程导入、构建产物是否可删除、常见注意事项。
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)：第三方代码和许可边界说明。

## 编码说明

`COSTAS/` 和 `TxBoard/` 中部分 Keil 工程源码含有 GB2312/GBK 中文注释。请使用 GBK、GB2312 或 Windows 默认 ANSI 编码打开和保存，不要批量转换为 UTF-8。Markdown 文档本身使用 UTF-8。

## 许可

项目自有代码和文档使用 MIT License。仓库中随工程保留的 TI、Nuvoton、ARM、Keil/Pack 相关启动文件、链接脚本、设备头文件、驱动库和预编译库不由顶层 MIT License 重新授权，仍遵循各自文件头或上游发布包中的许可条款。发布或再分发前请阅读 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

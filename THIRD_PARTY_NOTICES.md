# 第三方声明

本仓库顶层 `LICENSE` 中的 MIT License 仅覆盖项目自有代码和文档。为了保留嵌入式工程的可导入性和可复现性，仓库中随工程保留了若干第三方启动文件、设备头文件、链接脚本、驱动库、预编译库和 IDE 工程元数据。这些文件不因本仓库采用 MIT License 而被重新授权，仍遵循各自文件头、上游发布包或对应工具链/芯片支持包中的许可条款。

发布、复制、修改或再分发本仓库时，应保留第三方文件中的原始版权声明、许可条件和免责声明。

## Texas Instruments MSP430 支持文件

涉及路径：

```text
AGC/lnk_msp430g2553.cmd
```

该链接脚本带有 Texas Instruments 版权声明和近似 BSD 风格的再分发许可。其条件包括：保留版权声明、许可条件和免责声明；未经书面许可，不得使用 Texas Instruments 或贡献者名称为衍生产品背书。

## Texas Instruments TM4C / TivaWare 支持文件

涉及路径：

```text
baseband/tm4c123gh6pm_startup_ccs.c
baseband/tm4c123gh6pm.cmd
TxBoard/inc/
TxBoard/driverlib/
```

`TxBoard/driverlib/readme.txt` 标明仓库中包含的 Tiva Peripheral Driver Library 版本为 `2.1.4.178`，并附带 Texas Instruments 的 BSD 风格软件许可。`TxBoard/inc/` 和 `TxBoard/driverlib/` 下大量头文件、源文件和构建文件也包含同类版权与许可声明。

`baseband/tm4c123gh6pm_startup_ccs.c` 使用另一段 Texas Instruments 启动文件声明，其中说明该文件由 TI 提供，用于 TI 微控制器产品。该文件应视为 TI 授权的支持代码，而不是本项目 MIT 授权代码。

## ARM / CMSIS 启动支持文件

涉及路径：

```text
TxBoard/RTE/Device/TM4C1294NCPDT/startup_TM4C129.s
TxBoard/RTE/Device/TM4C1294NCPDT/system_TM4C129.c
TxBoard/RTE/_Target_1/RTE_Components.h
```

其中启动汇编文件含有 ARM Limited 版权声明，并说明其用于基于 Cortex-M 处理器的设备。这些文件通常由 Keil/CMSIS 设备支持流程生成或提供，应保留其原始条款。

## Nuvoton / CMSIS 支持文件

涉及路径：

```text
COSTAS/CMSIS/
COSTAS/Dependencies/
```

这些文件包含 Nuvoton Technology Corp. 以及 CMSIS 风格的设备支持声明，用于 NUC140/NUC1xx 平台的 Costas 控制器工程。应保留上游头部声明，不应将其视作被顶层 MIT License 重新授权。

## Keil 工程和设备包元数据

涉及路径：

```text
COSTAS/project0904.uvproj
COSTAS/project0904.uvopt
COSTAS/project0904.sct
TxBoard/dds-ser.uvprojx
TxBoard/dds-ser.uvoptx
TxBoard/driverlib/driverlib.uvproj
TxBoard/driverlib/driverlib.uvopt
```

这些文件用于 Keil uVision 工程导入、目标配置、链接布局或设备包集成。它们保留在仓库中是为了方便复现工程结构，但其中由工具链或设备包生成的内容仍受对应厂商条款约束。

## TM1638 教学驱动

涉及路径：

```text
TxBoard/tm1638.c
TxBoard/tm1638.h
```

文件头部标明该驱动版权归属为 `2020-2021` 年上海交通大学电子工程系实验教学中心，但当前文件头没有给出明确的开源再分发许可证。若项目准备面向更广泛公众长期发布，建议向原作者或课程方确认授权，或替换为许可证明确的 TM1638 驱动实现。

## 项目自有代码

除上述第三方文件以及其他文件头另有说明的内容外，本项目自有固件代码和文档按顶层 MIT License 发布。

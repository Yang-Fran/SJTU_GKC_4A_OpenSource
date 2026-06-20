# 工程导入和构建复核

本仓库清理了构建输出，但保留了工程元数据和必要源码。结论是：具备完整工具链和芯片支持包时，其他人可以直接导入对应工程；被删除的构建文件不影响正常导入。

## AGC

- 工具链：TI Code Composer Studio。
- 目标芯片：MSP430G2553。
- 关键文件：
  - `.ccsproject`
  - `.cproject`
  - `.project`
  - `targetConfigs/MSP430G2553.ccxml`
  - `lnk_msp430g2553.cmd`
  - `main.c`
- 可删除内容：`Debug/`、`*.obj`、`*.out`、`*.map`、`*.d`。

导入方式：在 CCS 中选择导入现有 CCS/Eclipse 工程，目录指向 `AGC/`。

## baseband

- 工具链：TI Code Composer Studio。
- 目标芯片：TM4C123GH6PM。
- 关键文件：
  - `.ccsproject`
  - `.cproject`
  - `.project`
  - `targetConfigs/Tiva TM4C123GH6PM.ccxml`
  - `tm4c123gh6pm.cmd`
  - `tm4c123gh6pm_startup_ccs.c`
  - `main.c`
- 可删除内容：`Debug/`、`.launches/`、`.claude/`、`*.obj`、`*.out`、`*.map`、`*.d`。

导入方式：在 CCS 中导入 `baseband/`。如果 CCS 版本或 TivaWare 安装路径不同，可能需要重新选择编译器版本或调整 include path。

## TxBoard

- 工具链：Keil uVision。
- 目标芯片：TM4C1294NCPDT。
- 关键文件：
  - `dds-ser.uvprojx`
  - `dds-ser.uvoptx`
  - `RTE/`
  - `inc/`
  - `driverlib/`
  - `dds_ser_main.c`
  - `ad9850_serial.c/.h`
  - `tm1638.c/.h`
- 可删除内容：`Objects/`、`Listings/`、`*.uvguix.*`、`*.map`、`*.dep`、`*.o`、`*.crf`、`*.axf`。

导入方式：在 Keil uVision 中打开 `TxBoard/dds-ser.uvprojx`。首次构建会重新生成 `Objects/` 和 `Listings/`。

## COSTAS

- 工具链：Keil uVision。
- 目标芯片：Nuvoton NUC140/NUC1xx 系列。
- 关键文件：
  - `project0904.uvproj`
  - `project0904.uvopt`
  - `project0904.sct`
  - `CMSIS/`
  - `Dependencies/`
  - `BoardSupport/`
  - `COSTASProject/`
  - `CMakeLists.txt`，仅用于索引，不是权威构建入口
- 可删除内容：`objects/`、`cmake-build-debug/`、`.idea/`、`.vscode/`、`*.uvgui*`、`*.map`、`*.dep`、`*.lst`、`*.bak`。

导入方式：在 Keil uVision 中打开 `COSTAS/project0904.uvproj`。如果本机没有 Nuvoton 设备包或 Keil include 路径不同，需要安装相应 Pack 或重新配置工具链路径。

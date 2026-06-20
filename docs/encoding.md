# 文件编码说明

本项目包含 Keil uVision 时代遗留的中文注释源码，尤其是 `COSTAS/` 和 `TxBoard/` 两个 Keil 工程。此类文件在 Windows/Keil 环境下通常按 GB2312/GBK，也就是 Windows 代码页 936，打开和保存。

## 结论

- `COSTAS/` 和 `TxBoard/` 中的部分 `.c/.h/.uvproj/.uvopt` 文件应按 GB2312/GBK 处理。
- 不要使用编辑器的自动 UTF-8 转换功能批量保存这些文件。
- 若只阅读代码，建议在 VS Code、Notepad++、CLion 或 Keil 中显式选择 GBK/GB2312 打开。
- 若需要将源码统一转换为 UTF-8，应单独开一次编码迁移，并在真实工具链上重新导入、编译和下载验证。

## 已复核现象

以 `COSTAS/COSTASProject/costas_loop.h` 和 `TxBoard/dds_ser_main.c` 为例：

- 使用 Windows 默认 ANSI/GBK 读取时，中文注释正常。
- 使用 UTF-8 读取时，中文注释会显示为乱码。

这说明当前工程编码与 Keil/Windows 中文工程习惯一致，不是构建产物或隐私问题。

## 对贡献者的建议

1. 修改 Keil 工程源码前，先确认编辑器底部显示的编码为 GBK、GB2312 或系统默认 ANSI。
2. 不要在保存时点击“转换为 UTF-8”。
3. 文档文件，例如 `README.md`、`docs/*.md`、`THIRD_PARTY_NOTICES.md`，使用 UTF-8。
4. 如果 GitHub 网页预览中源码中文注释乱码，以本地 GBK/GB2312 打开结果为准。
5. 代码功能修改和编码迁移应分开提交，方便定位问题。

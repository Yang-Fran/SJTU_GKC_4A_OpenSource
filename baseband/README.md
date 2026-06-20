# 接收基带固件

`baseband/` 是 TM4C123GH6PM 接收基带处理程序。它接收 Costas 环输出后的基带模拟信号，通过 ADC 采样、数字滤波、位同步、符号判决和差分译码恢复原始比特流，并提供若干 GPIO/DAC 调试输出。

## 文件结构

```text
baseband/
|-- main.c                         接收基带处理主程序
|-- tm4c123gh6pm_startup_ccs.c      TI CCS 启动文件
|-- tm4c123gh6pm.cmd                TI 链接脚本
|-- targetConfigs/                  CCS 目标配置
|-- *.jpg, *.png                    调试波形和参考图
|-- .ccsproject                     CCS 工程元数据
|-- .cproject                       Eclipse CDT 工程元数据
`-- .project                        Eclipse 工程元数据
```

## 核心算法

主处理链路由定时器中断驱动：

1. 从 `PE1/AIN2` 读取 ADC0 采样值。
2. 使用三点中值滤波抑制孤立毛刺。
3. 使用短窗平均滤波和 Schmitt 判决提取 NRZ 边沿。
4. 将边沿事件送入软件 DPLL，修正本地码元边界。
5. 在码元同步窗口内执行 integrate-and-dump 积分判决。
6. 对相邻判决结果做差分译码，恢复发送端原始数据。
7. 输出恢复位时钟、判决结果、译码结果和 DAC7611 观测波形。

关键参数集中在 `main.c` 顶部，包括 `SYMBOL_RATE_HZ`、`OSR_N`、边沿滤波长度、门限跟踪参数和 DPLL 死区。

## 引脚说明

- `PE1/AIN2`：接收基带模拟输入。
- `PE2`：恢复出的位时钟输出。
- `PA2`：采样判决输出。
- `PA3`：差分译码输出。
- `PD0`：DAC7611 `/CS`。
- `PD1`：DAC7611 `CLK`。
- `PD2`：DAC7611 `SDI`。
- `PD3`：DAC7611 `/LD`。
- `PF1`：中断执行时间观测引脚。

## 导入和构建

使用 TI Code Composer Studio 导入本目录工程，目标器件选择 TM4C123GH6PM。`Debug/`、`*.map`、`*.out`、`*.obj` 等均为构建产物，删除不影响导入；首次构建会自动重新生成。

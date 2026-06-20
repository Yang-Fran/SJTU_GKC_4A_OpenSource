#ifndef APP_STATE_H
#define APP_STATE_H

// 项目业务状态管理模块，负责在 UI 选择的功能之间切换，并周期调用当前功能任务。
// 基础状态: 0 空闲，1 双路独立频率，2 同频可调相位，3 频率校准，4 BPSK，
//           5 对角QPSK，6 T点标定，7 轴向三态QPSK。
// 模式参数: mode1_signal1_freq 和 mode1_signal2_freq 是模式 1 的双路频率，
//          mode2_freq 和 mode2_phase_diff 是模式 2 的频率和相位差。
// 模式切换时会停止其余 Costas/校准任务，避免多个任务同时改写 AD9850 输出。

#include <stdint.h>

extern unsigned int mode1_signal1_freq;
extern unsigned int mode1_signal2_freq;

extern unsigned int mode2_freq;
extern unsigned int mode2_phase_diff;

// ------------------------------
// 状态机业务和切换
// ------------------------------

// 每次主循环调用一次，并根据当前 mode 执行业务任务。
// 模式 1/2 刷新测试输出；模式 3/6 分别运行频率校准与 T 点标定；模式 4/5/7 运行解调闭环。
void AppState_Task(void);

// 只切换当前业务模式，不触发 AD9850 参数刷新。
// current_mode: 取值 0..7，对应本文件顶部列出的业务状态。
void AppState_SelectMode(unsigned int current_mode);

// 请求进入模式 1：两路独立频率输出。
// signal1_freq/signal2_freq: 两路输出频率，单位 KHz。
void AppState_StartMode1(unsigned int signal1_freq, unsigned int signal2_freq);

// 请求进入模式 2：同频双路输出，并设置第二路相位差。
// signal_freq: 两路共同输出频率，单位 KHz；phase_diff: 相位步进值，每步 11.25 度。
void AppState_StartMode2(unsigned int signal_freq, unsigned int phase_diff);


// ------------------------------
// 闭环与校准模式入口
// ------------------------------

// 请求进入频率校准模式：外部输入 100kHz 参考，与本地约 99.9kHz 正交本振形成小拍频。
void AppState_StartCalibration(void);

// 请求进入 T 点 ADC/IQ 标定模式。
void AppState_StartTPointCalibration(void);
// 请求进入 BPSK 复相位闭环模式。
void AppState_StartBpsk(void);
// 请求进入对角星座 QPSK 闭环，目标点为 (±1,±1)。
void AppState_StartQpskDiagonal(void);
// 请求进入轴向三态 QPSK 闭环，目标点为 (±1,0)/(0,±1)。
void AppState_StartQpskAxis(void);
// 停止当前业务模式，回到空闲状态。
void AppState_Stop(void);

#endif

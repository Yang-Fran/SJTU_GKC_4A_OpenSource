#ifndef COSTAS_LOOP_H
#define COSTAS_LOOP_H

// Costas 解调闭环、DDS 参考时钟校准和 T 点标定接口。
//
// 每次 ADC 采样组成复平面点 T[n] = I[n] + jQ[n]。原始 I/Q 不做长窗口平均，
// 只对相位误差和相位速度低通，避免平均掉 BPSK/QPSK 调制数据。
//
// BPSK/QPSK 解调闭环：
// 1. 将 T 点判决到最近标准星座点，使用沿标准点方向投影 A 和垂直投影 B，
//    计算 phase_error = B/A * 1024，近似表示相对标准点的角度误差。
// 2. phase_speed 是相邻相位误差之差的低通值，仅保留作调试观察。
// 3. BPSK/QPSK 运行期不再调整频率，只按 11.25 度步进追踪相位漂移。
// 4. T 点低功率、靠近判决边界或发生漏采样时，不更新对应误差历史。
//
// FREQ CAL 使用外部准确 100kHz 与本地约 99.9kHz 正交本振形成 100Hz 拍频。
// 对相邻 T 点 T0、T1：
//     dot   = I0*I1 + Q0*Q1 = |T0||T1|cos(delta)
//     cross = I0*Q1 - Q0*I1 = |T0||T1|sin(delta)
// 因而 cross/dot = tan(delta)，可在不使用浮点 atan 的情况下估计每采样转角。
// FREQ CAL 只拒绝绝对值极端的坏点，再平均 128 个有效瞬时 speed。
// 当前使用经实机验证的 speed=cross/dot*1024；约 5ksample/s、100Hz 拍频时，
// 实机顺时针旋转得到的目标 SPD 约为 -129。
//
// T POINT CAL 根据两路 ADC 的 MIN/MAX 计算 MID 和 AMP，用于修正直流中心与增益差。
// I 对应 ADC5/GPA5，Q 对应 ADC6/GPA6；该标定不修正 I/Q 正交相位误差。
// 更新时间：2026.06 氧化物

#include <stdint.h>

// 三种解调模式共享相位 bang-bang 控制，区别仅在最近标准星座点的判决方式。
typedef enum {
    COSTAS_MOD_BPSK = 0,
    COSTAS_MOD_QPSK_DIAGONAL, // 标准点为 (±1,±1)，两路输出均为双极性数据
    COSTAS_MOD_QPSK_AXIS      // 标准点为 (±1,0)/(0,±1)，两路输出为 0/±1 三态
} CostasModulation;


// ------------------------------
// COSTAS 基本工作功能
// ------------------------------

// 启动指定星座的复相位闭环，并从零相位开始捕获。
void Costas_Start(CostasModulation modulation);
// 停止解调闭环并清空历史量。
void Costas_Stop(void);
// 消费一次新的连续 ADC 原始样本，运行 BPSK/QPSK 共享闭环。
// 处理顺序为：
// ADC 归一化 -> 星座判决和相位误差 -> 相位误差低通 -> bang-bang 相位步进
// -> 连续满足条件后报告锁定。
// 漏采样、低功率或靠近判决边界时会丢弃速度历史，避免产生虚假频偏。
void Costas_Task(void);
// 返回解调闭环是否连续满足锁定条件。
uint8_t Costas_IsLocked(void);
// 返回低通后的相位误差，供调试显示或参数整定。
int32_t Costas_GetPhaseError(void);
// 返回低通后的相位速度，供调试显示或参数整定。
int32_t Costas_GetPhaseSpeed(void);


// ------------------------------
// DDS 参考时钟校准
// ------------------------------


// 启动频率校准：本地两路 DDS 请求输出 99.9kHz 正交本振，与外部 100kHz 形成小拍频。
void Costas_CalibrationStart(void);
// 停止频率校准并丢弃相邻采样历史。
void Costas_CalibrationStop(void);
// 运行频率校准任务；根据拍频速度分段调整 DDS 公共参考时钟估计值。
void Costas_CalibrationTask(void);
// 返回校准拍频速度是否连续达到目标范围。
uint8_t Costas_CalibrationIsLocked(void);
// 返回当前 DDS 公共参考时钟估计值，单位 Hz。
uint32_t Costas_CalibrationGetReferenceHz(void);
// 返回低通后的原始拍频速度；当前硬件目标约为 -129。
int32_t Costas_CalibrationGetSpeed(void);

// ------------------------------
// T 点 ADC/IQ 标定
// ------------------------------

// 启动/停止 T 点标定，并周期统计两路 ADC 的 MIN/MAX。
void Costas_TPointCalibrationStart(void);
void Costas_TPointCalibrationStop(void);
void Costas_TPointCalibrationTask(void);
// 返回本轮测量是否已产生有效 MID/AMP。
uint8_t Costas_TPointCalibrationIsValid(void);
// channel=0 返回 I/ADC5，channel=1 返回 Q/ADC6。
uint32_t Costas_TPointGetMin(uint8_t channel);
uint32_t Costas_TPointGetMax(uint8_t channel);
int32_t Costas_TPointGetMid(uint8_t channel);
int32_t Costas_TPointGetAmp(uint8_t channel);

#endif

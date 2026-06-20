#include <stdint.h>
#include "AD9850_func.h"
#include "DrvGPIO.h"
#include "adc_sample.h"
#include "costas_loop.h"

// Costas 解调闭环与校准实现，算法和公开接口说明见 costas_loop.h。

// ADC/IQ 默认标定参数，T 点标定成功后自动覆盖。
#define LOOP_I_MID_DEFAULT         535
#define LOOP_Q_MID_DEFAULT         535
#define LOOP_I_AMP_DEFAULT         225
#define LOOP_Q_AMP_DEFAULT         225
// 归一化公式为 (ADC - MID) * LOOP_SCALE / AMP，理想峰值约落在正负 256。
#define LOOP_SCALE                 256
// 12 位 ADC 编码上界，用于初始化 T 点最小值统计。
#define TPOINT_ADC_MAX_CODE        4095
// T 点单路最小峰峰跨度，跨度过小时 MID/AMP 估计容易被噪声主导。
#define TPOINT_MIN_SPAN            64
// 至少观察 256 个 T 点后才接受本轮 MID/AMP 参数。
#define TPOINT_MIN_SAMPLES         256

// 解调环阈值和滤波参数。
#define LOOP_BASE_FREQ_HZ          100000UL
#define LOOP_ERROR_SCALE           1024
#define LOOP_POWER_MIN             4096
#define LOOP_AXIS_MIN              48
#define LOOP_QPSK_DIAG_MARGIN_MIN  28 // 对角 QPSK：离 I/Q 坐标轴太近时不更新
#define LOOP_QPSK_AXIS_MARGIN_MIN  28 // 轴向 QPSK：离 |I|=|Q| 判决边界太近时不更新

#define LOOP_PHASE_AVG_SHIFT       3
#define LOOP_SPEED_AVG_SHIFT       4
#define LOOP_PHASE_STEP_TH         160
#define LOOP_PHASE_LOCK_TH         80
#define LOOP_PHASE_FREEZE_N        8
#define LOOP_LOCK_COUNT            64

// 实机相位控制方向标定入口。
#define LOOP_PHASE_DIRECTION       -1

// FREQ CAL 参数。
#define CAL_EXTERNAL_FREQ_HZ       100000UL
#define CAL_BEAT_FREQ_HZ           100
#define CAL_LOCAL_FREQ_HZ          (CAL_EXTERNAL_FREQ_HZ - CAL_BEAT_FREQ_HZ)
// 使用经实机验证的 1024 倍速度尺度；高分辨率尺度会放大 I/Q 非正交造成的系统偏差。
#define CAL_SPEED_SCALE            1024
#define CAL_TARGET_SPEED           129
// 实机 T 点顺时针旋转，控制前先翻转原始负 SPD。
#define CAL_SPEED_DIRECTION        -1
#define CAL_MEASURE_COUNT          128
#define CAL_SPEED_RAW_MAX          256
#define CAL_SPEED_AVG_SHIFT        2
// 1 个 speed 单位约对应 0.77Hz 拍频误差，这是当前 T 点角速度法的可靠分辨率。
#define CAL_ADJUST_DEADBAND        1
#define CAL_ADJUST_COUNT           8
#define CAL_ACC_LIMIT              1048576
// 每个 speed 误差修正约 512Hz REF，约为估计误差的一半。
#define CAL_REF_HZ_PER_SPEED       512
#define CAL_REF_STEP_LIMIT         2048
#define CAL_SPEED_LOCK             1
#define CAL_LOCK_COUNT             16

#define C12_LED_H                  DrvGPIO_SetBit(E_GPC,12)
#define C12_LED_L                  DrvGPIO_ClrBit(E_GPC,12)
#define LED_FAST_TICKS             64
#define LED_SLOW_TICKS             256

// 锁相环的控制状态机
typedef enum {
    LOOP_STATE_INVALID = 0,
    LOOP_STATE_TRACK_PHASE,
    LOOP_STATE_SETTLE,
    LOOP_STATE_LOCKED
} LoopState;

// 解调闭环状态：phase_err_avg 表示相对最近标准星座点的相位误差，
// phase_speed_avg 表示相位误差长期斜率，也就是残余频偏。
static CostasModulation loop_modulation = COSTAS_MOD_BPSK;
static LoopState loop_state = LOOP_STATE_INVALID;
static uint8_t loop_running;
static uint8_t loop_locked;
static uint8_t phase_word;
static uint8_t phase_cooldown;
static uint16_t lock_count;
static uint16_t last_sample_sequence;
static int32_t phase_err_avg;
static int32_t phase_err_prev;
static int32_t phase_speed_avg;
static uint8_t phase_prev_valid;

// 当前生效的 ADC/IQ 标定参数；T 点标定有效后自动替换默认值。
static int32_t loop_i_mid = LOOP_I_MID_DEFAULT;
static int32_t loop_q_mid = LOOP_Q_MID_DEFAULT;
static int32_t loop_i_amp = LOOP_I_AMP_DEFAULT;
static int32_t loop_q_amp = LOOP_Q_AMP_DEFAULT;

// 校准闭环状态：保存相邻复采样点以及拍频速度积分器。
static uint8_t calibration_running;
static uint8_t calibration_locked;
static uint16_t calibration_lock_count;
static uint16_t calibration_last_sequence;
static uint8_t calibration_prev_valid;
static int32_t calibration_prev_i;
static int32_t calibration_prev_q;
static int32_t calibration_speed_avg;
// 保留速度低通的小数余量，避免负数右移造成稳态停滞误差。
static int32_t calibration_speed_filter_acc;
static int64_t calibration_speed_sum;
static uint16_t calibration_measure_count;
static int32_t calibration_acc;
static uint16_t calibration_adjust_count;

// T 点标定状态：由 QPSK 四象限点的原始 ADC 范围估计两路中心和幅度。
static uint8_t tpoint_running;
static uint8_t tpoint_valid;
static uint16_t tpoint_last_sequence;
static uint32_t tpoint_sample_count;
static uint32_t tpoint_min[2];
static uint32_t tpoint_max[2];

static uint16_t led_count;
static uint8_t led_on;

// 1 将有符号整数限制在正负 limit 范围内。
// 2 value: 待限制数值；limit: 正向最大幅度。
static int32_t Limit32(int32_t value, int32_t limit) {
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

// 1 返回 32 位有符号整数的绝对值。
static int32_t Abs32(int32_t value) {
    return value < 0 ? -value : value;
}

// 1 将 ADC Code 按通道中心和幅度归一化到约正负 LOOP_SCALE。
// 2 adc: 原始 ADC 值；mid: 直流中心；amp: 单边有效幅度。
static int32_t NormalizeAdc(uint32_t adc, int32_t mid, int32_t amp) {
    return ((int32_t) adc - mid) * LOOP_SCALE / amp;
}

// 1 根据输入有效性和锁定结果刷新 C12 LED。
// 2 locked: 非 0 时常亮；valid: 非 0 时慢闪，否则快闪。
static void UpdateLed(uint8_t locked, uint8_t valid) {
    uint16_t toggle_ticks;

    if (locked) {
        led_count = 0;
        led_on = 1;
        C12_LED_L;
        return;
    }

    toggle_ticks = valid ? LED_SLOW_TICKS : LED_FAST_TICKS;
    if (++led_count >= toggle_ticks) {
        led_count = 0;
        led_on = !led_on;
    }
    if (led_on) C12_LED_L;
    else C12_LED_H;
}

// 两路固定使用校准后的 100kHz，第二路相位字加 8，形成 90 度正交本振。
static void UpdateLoopOutput(void) {
    setup_AD9850_Hz(LOOP_BASE_FREQ_HZ, LOOP_BASE_FREQ_HZ,
        phase_word, (phase_word + 8) & 0x1f);
}

// 清空解调闭环历史量，避免退出后再次进入时继承旧误差。
static void ResetTracker(void) {
    loop_state = LOOP_STATE_INVALID;
    loop_locked = 0;
    phase_cooldown = 0;
    lock_count = 0;
    phase_err_avg = 0;
    phase_err_prev = 0;
    phase_speed_avg = 0;
    phase_prev_valid = 0;
}






// A/B 分别是 T 点沿标准点方向和垂直方向的投影，B/A 表示小角度相位误差。
static uint8_t CalculatePhaseError(int32_t i, int32_t q, int32_t *phase_error) {
    int64_t power = (int64_t) i * i + (int64_t) q * q;
    int32_t abs_i = Abs32(i);
    int32_t abs_q = Abs32(q);
    int32_t a;
    int32_t b;

    if (power < LOOP_POWER_MIN) return 0;

    if (loop_modulation == COSTAS_MOD_BPSK) {
        if (abs_i < LOOP_AXIS_MIN) return 0;
        a = abs_i;
        b = i >= 0 ? q : -q;
    } else if (loop_modulation == COSTAS_MOD_QPSK_DIAGONAL) {
        // 对角 QPSK 最近点为 (sign(I), sign(Q))。
        // A 是 T 点在该对角线上的投影，B 是相对该对角线的垂直偏差。
        if ((abs_i < abs_q ? abs_i : abs_q) < LOOP_QPSK_DIAG_MARGIN_MIN) return 0;
        a = abs_i + abs_q;
        b = (i >= 0 ? q : -q) - (q >= 0 ? i : -i);
    } else if (loop_modulation == COSTAS_MOD_QPSK_AXIS) {
        // 轴向 QPSK 最近点为 (±1,0) 或 (0,±1)，适合后级读取两路 0/±1 三态。
        // |I|-|Q| 决定最近坐标轴；靠近两轴的等距边界时暂停更新，避免误判。
        if (Abs32(abs_i - abs_q) < LOOP_QPSK_AXIS_MARGIN_MIN) return 0;
        if (abs_i >= abs_q) {
            a = abs_i;
            b = i >= 0 ? q : -q;
        } else {
            a = abs_q;
            b = -(q >= 0 ? i : -i);
        }
    } else {
        return 0;
    }

    if (a < LOOP_AXIS_MIN) return 0;
    *phase_error = (int32_t) ((int64_t) b * LOOP_ERROR_SCALE / a);
    return 1;
}

// 按长期相位偏差执行一档 11.25 度相位修正。
// AD9850 只能用 5 位相位字离散调相，所以这里使用 bang-bang 控制：
// 长期误差超过阈值时只跳一档，然后重新观察，而不是连续积分相位字。
// 跳相后清空速度历史并进入冻结窗口，避免把相位阶跃误判为频偏。
static void ApplyPhaseControl(void) {
    int8_t step = 0;

    if (phase_err_avg > LOOP_PHASE_STEP_TH) step = LOOP_PHASE_DIRECTION;
    else if (phase_err_avg < -LOOP_PHASE_STEP_TH) step = -LOOP_PHASE_DIRECTION;
    if (step == 0) return;

    phase_word = (phase_word + (step > 0 ? 1 : 31)) & 0x1f;
    phase_err_avg = 0;
    phase_speed_avg = 0;
    phase_prev_valid = 0;
    phase_cooldown = LOOP_PHASE_FREEZE_N;
    lock_count = 0;
    loop_locked = 0;
    loop_state = LOOP_STATE_SETTLE;
}




void Costas_CalibrationStart(void) {
    calibration_running = 1;
    calibration_locked = 0;
    calibration_lock_count = 0;
    calibration_prev_valid = 0;
    calibration_speed_avg = 0;
    calibration_speed_filter_acc = 0;
    calibration_speed_sum = 0;
    calibration_measure_count = 0;
    calibration_acc = 0;
    calibration_adjust_count = 0;
    calibration_last_sequence = ADC0_SampleSequence;
    setup_AD9850_Hz(CAL_LOCAL_FREQ_HZ, CAL_LOCAL_FREQ_HZ, 0, 8);
}
void Costas_CalibrationStop(void) {
    calibration_running = 0;
    calibration_prev_valid = 0;
}
void Costas_CalibrationTask(void) {
    uint16_t sample_sequence;
    uint16_t sample_delta;
    int32_t i;
    int32_t q;
    int64_t dot;
    int64_t cross;
    int32_t speed;
    int32_t speed_error;
    int32_t speed_error_avg;
    uint32_t reference_hz;
    int32_t step;

    if (!calibration_running) return;
    sample_sequence = ADC0_SampleSequence;
    sample_delta = (uint16_t) (sample_sequence - calibration_last_sequence);
    if (sample_delta == 0) return;
    calibration_last_sequence = sample_sequence;
    if (sample_delta != 1) {
        // 无法用不相邻的 T 点计算单采样转角，只丢弃本次点对。
        // 保留已经累计的速度误差，否则 UI 刷新等偶发延迟会让 REF 永远无法调整。
        calibration_prev_valid = 0;
        calibration_locked = 0;
        calibration_lock_count = 0;
        return;
    }
    i = NormalizeAdc(ADC0_Value[0], loop_i_mid, loop_i_amp);
    q = NormalizeAdc(ADC0_Value[1], loop_q_mid, loop_q_amp);

    if ((int64_t) i * i + (int64_t) q * q < LOOP_POWER_MIN) {
        calibration_prev_valid = 0;
        calibration_locked = 0;
        calibration_lock_count = 0;
        calibration_speed_sum = 0;
        calibration_measure_count = 0;
        calibration_acc = 0;
        calibration_adjust_count = 0;
        UpdateLed(0, 0);
        return;
    }

    if (!calibration_prev_valid) {
        calibration_prev_i = i;
        calibration_prev_q = q;
        calibration_prev_valid = 1;
        return;
    }

    dot = (int64_t) calibration_prev_i * i + (int64_t) calibration_prev_q * q;
    cross = (int64_t) calibration_prev_i * q - (int64_t) calibration_prev_q * i;
    calibration_prev_i = i;
    calibration_prev_q = q;
    if (dot <= LOOP_AXIS_MIN) {
        calibration_prev_valid = 0;
        return;
    }

    // 只拒绝绝对值极端的速度坏点，再平均一组有效瞬时速度。
    // 不按旋转方向或最小幅度筛选，否则会单边丢弃偏小样本并把平均 SPD 推大。
    speed = (int32_t) (cross * CAL_SPEED_SCALE / dot);
    if (Abs32(speed) > CAL_SPEED_RAW_MAX) return;
    calibration_speed_sum += speed;
    if (++calibration_measure_count < CAL_MEASURE_COUNT) return;

    speed = (int32_t) (calibration_speed_sum / CAL_MEASURE_COUNT);
    calibration_speed_sum = 0;
    calibration_measure_count = 0;

    // 用高分辨率累加器实现 y += (x-y)/4，保留整数除法丢失的小数余量。
    calibration_speed_filter_acc += speed -
        (calibration_speed_filter_acc >> CAL_SPEED_AVG_SHIFT);
    calibration_speed_avg = calibration_speed_filter_acc >> CAL_SPEED_AVG_SHIFT;
    speed_error = CAL_SPEED_DIRECTION * calibration_speed_avg - CAL_TARGET_SPEED;
    if (Abs32(speed_error) <= CAL_SPEED_LOCK) {
        if (calibration_lock_count < CAL_LOCK_COUNT) calibration_lock_count++;
        if (calibration_lock_count >= CAL_LOCK_COUNT) {
            calibration_locked = 1;
            UpdateLed(1, 1);
            return;
        }
    } else {
        calibration_lock_count = 0;
        calibration_locked = 0;
    }

    // 误差进入死区后彻底清空调整历史，REF 保持不变，不让小偏差无限积分。
    if (Abs32(speed_error) <= CAL_ADJUST_DEADBAND) {
        calibration_acc = 0;
        calibration_adjust_count = 0;
    } else {
        calibration_acc = Limit32(calibration_acc + speed_error, CAL_ACC_LIMIT);
        if (calibration_adjust_count < CAL_ADJUST_COUNT) calibration_adjust_count++;
    }

    if (calibration_adjust_count >= CAL_ADJUST_COUNT) {
        speed_error_avg = calibration_acc / CAL_ADJUST_COUNT;
        step = Limit32(speed_error_avg * CAL_REF_HZ_PER_SPEED, CAL_REF_STEP_LIMIT);

        // 正 speed_error 表示拍频过高、本振偏低；减小 REF 估计值会增大 FTW，
        // 从而提高本振频率。负误差反向处理。
        reference_hz = AD9850_GetReferenceHz();
        if (step > 0 && reference_hz > (uint32_t) step) reference_hz -= (uint32_t) step;
        else if (step < 0) reference_hz += (uint32_t) (-step);
        AD9850_SetReferenceHz(reference_hz);
        calibration_acc = 0;
        calibration_adjust_count = 0;
        calibration_prev_valid = 0;
        calibration_speed_sum = 0;
        calibration_measure_count = 0;
        calibration_lock_count = 0;
        calibration_locked = 0;
        setup_AD9850_Hz(CAL_LOCAL_FREQ_HZ, CAL_LOCAL_FREQ_HZ, 0, 8);
    }

    UpdateLed(calibration_locked, 1);
}


uint8_t Costas_CalibrationIsLocked(void) {
    return calibration_locked;
}

uint32_t Costas_CalibrationGetReferenceHz(void) {
    return AD9850_GetReferenceHz();
}

int32_t Costas_CalibrationGetSpeed(void) {
    return calibration_speed_avg;
}

void Costas_TPointCalibrationStart(void) {
    tpoint_running = 1;
    tpoint_valid = 0;
    tpoint_sample_count = 0;
    tpoint_last_sequence = ADC0_SampleSequence;
    tpoint_min[0] = tpoint_min[1] = TPOINT_ADC_MAX_CODE;
    tpoint_max[0] = tpoint_max[1] = 0;
    setup_AD9850_Hz(LOOP_BASE_FREQ_HZ, LOOP_BASE_FREQ_HZ, 0, 8);
}

void Costas_TPointCalibrationStop(void) {
    tpoint_running = 0;
}

void Costas_TPointCalibrationTask(void) {
    uint16_t sample_sequence;
    uint8_t channel;
    uint32_t span_i;
    uint32_t span_q;

    if (!tpoint_running) return;
    sample_sequence = ADC0_SampleSequence;
    if (sample_sequence == tpoint_last_sequence) return;
    tpoint_last_sequence = sample_sequence;

    for (channel = 0; channel < 2; channel++) {
        if (ADC0_Value[channel] < tpoint_min[channel]) tpoint_min[channel] = ADC0_Value[channel];
        if (ADC0_Value[channel] > tpoint_max[channel]) tpoint_max[channel] = ADC0_Value[channel];
    }
    if (tpoint_sample_count < 0xffffffffu) tpoint_sample_count++;

    span_i = tpoint_max[0] - tpoint_min[0];
    span_q = tpoint_max[1] - tpoint_min[1];
    if (tpoint_sample_count >= TPOINT_MIN_SAMPLES &&
        span_i >= TPOINT_MIN_SPAN && span_q >= TPOINT_MIN_SPAN) {
        loop_i_mid = (int32_t) ((tpoint_max[0] + tpoint_min[0]) / 2);
        loop_q_mid = (int32_t) ((tpoint_max[1] + tpoint_min[1]) / 2);
        loop_i_amp = (int32_t) (span_i / 2);
        loop_q_amp = (int32_t) (span_q / 2);
        tpoint_valid = 1;
    }
}

uint8_t Costas_TPointCalibrationIsValid(void) {
    return tpoint_valid;
}

uint32_t Costas_TPointGetMin(uint8_t channel) {
    return channel < 2 ? tpoint_min[channel] : 0;
}

uint32_t Costas_TPointGetMax(uint8_t channel) {
    return channel < 2 ? tpoint_max[channel] : 0;
}

int32_t Costas_TPointGetMid(uint8_t channel) {
    if (channel == 0) return loop_i_mid;
    if (channel == 1) return loop_q_mid;
    return 0;
}

int32_t Costas_TPointGetAmp(uint8_t channel) {
    if (channel == 0) return loop_i_amp;
    if (channel == 1) return loop_q_amp;
    return 0;
}

void Costas_Start(CostasModulation modulation) {
    loop_modulation = modulation;
    loop_running = 1;
    last_sample_sequence = ADC0_SampleSequence;
    phase_word = 0;
    ResetTracker();
    UpdateLoopOutput();
}
void Costas_Stop(void) {
    loop_running = 0;
    ResetTracker();
}
void Costas_Task(void) {
    uint16_t sample_sequence;
    uint16_t sample_delta;
    int32_t i;
    int32_t q;
    int32_t phase_error;
    int32_t speed_raw;

    if (!loop_running) return;
    sample_sequence = ADC0_SampleSequence;
    sample_delta = (uint16_t) (sample_sequence - last_sample_sequence);
    if (sample_delta == 0) return;
    last_sample_sequence = sample_sequence;
    if (sample_delta != 1) {
        phase_prev_valid = 0;
        loop_locked = 0;
        lock_count = 0;
        return;
    }

    i = NormalizeAdc(ADC0_Value[0], loop_i_mid, loop_i_amp);
    q = NormalizeAdc(ADC0_Value[1], loop_q_mid, loop_q_amp);

    if (phase_cooldown != 0) {
        phase_cooldown--;
        phase_prev_valid = 0;
        loop_state = LOOP_STATE_SETTLE;
        UpdateLed(0, 1);
        return;
    }

    if (!CalculatePhaseError(i, q, &phase_error)) {
        phase_prev_valid = 0;
        loop_locked = 0;
        lock_count = 0;
        loop_state = LOOP_STATE_INVALID;
        UpdateLed(0, 0);
        return;
    }

    // 一阶低通只作用于误差量，不直接平均 I/Q 原始调制点。
    phase_err_avg += (phase_error - phase_err_avg) >> LOOP_PHASE_AVG_SHIFT;
    if (!phase_prev_valid) {
        phase_err_prev = phase_error;
        phase_prev_valid = 1;
        UpdateLed(0, 1);
        return;
    }

    // 相邻相位误差之差是每采样转角，长期非零即表示残余频偏。
    speed_raw = phase_error - phase_err_prev;
    phase_err_prev = phase_error;
    phase_speed_avg += (speed_raw - phase_speed_avg) >> LOOP_SPEED_AVG_SHIFT;

    loop_state = LOOP_STATE_TRACK_PHASE;
    ApplyPhaseControl();
    if (phase_cooldown != 0) {
        UpdateLoopOutput();
        UpdateLed(0, 1);
        return;
    }
    if (Abs32(phase_err_avg) <= LOOP_PHASE_LOCK_TH) {
        if (lock_count < LOOP_LOCK_COUNT) lock_count++;
        if (lock_count >= LOOP_LOCK_COUNT) {
            loop_locked = 1;
            loop_state = LOOP_STATE_LOCKED;
        }
    } else {
        lock_count = 0;
        loop_locked = 0;
    }

    UpdateLoopOutput();
    UpdateLed(loop_locked, 1);
}
uint8_t Costas_IsLocked(void) {
    return loop_locked;
}
int32_t Costas_GetPhaseError(void) {
    return phase_err_avg;
}
int32_t Costas_GetPhaseSpeed(void) {
    return phase_speed_avg;
}

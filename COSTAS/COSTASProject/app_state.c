#include <stdint.h>
#include <stdlib.h>
#include "app_state.h"
#include "AD9850_func.h"
#include "costas_loop.h"

// 项目业务状态实现。公开模式说明见 app_state.h。
// 更新时间：2026.05 氧化物

static unsigned int mode = 0;
static unsigned int mode_change_flag = 0;

unsigned int mode1_signal1_freq = 100; // 单位为KHz
unsigned int mode1_signal2_freq = 100; // 单位为KHz

unsigned int mode2_freq = 100; // 单位为KHz
unsigned int mode2_phase_diff = 8; // 每步 11.25 度


void AppState_SelectMode(unsigned int current_mode) {
    Costas_Stop();
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    if (current_mode <= 7) {
        mode = current_mode;
        mode_change_flag = 0;
    } else {
        mode = 0;
        mode_change_flag = 0;
    }
}
void AppState_StartMode1(unsigned int signal1_freq, unsigned int signal2_freq) {
    Costas_Stop();
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    mode1_signal1_freq = signal1_freq;
    mode1_signal2_freq = signal2_freq;
    mode = 1;
    mode_change_flag = 1;
}
void AppState_StartMode2(unsigned int signal_freq, unsigned int phase_diff) {
    Costas_Stop();
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    mode2_freq = signal_freq;
    mode2_phase_diff = phase_diff;
    mode = 2;
    mode_change_flag = 1;
}


void AppState_StartCalibration(void) {
    mode = 3;
    mode_change_flag = 0;
    Costas_Stop();
    Costas_TPointCalibrationStop();
    Costas_CalibrationStart();
}
void AppState_StartTPointCalibration(void) {
    mode = 6;
    mode_change_flag = 0;
    Costas_Stop();
    Costas_CalibrationStop();
    Costas_TPointCalibrationStart();
}
void AppState_StartBpsk(void) {
    mode = 4;
    mode_change_flag = 0;
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    Costas_Start(COSTAS_MOD_BPSK);
}
void AppState_StartQpskDiagonal(void) {
    mode = 5;
    mode_change_flag = 0;
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    Costas_Start(COSTAS_MOD_QPSK_DIAGONAL);
}
void AppState_StartQpskAxis(void) {
    mode = 7;
    mode_change_flag = 0;
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    Costas_Start(COSTAS_MOD_QPSK_AXIS);
}
void AppState_Stop(void) {
    Costas_Stop();
    Costas_CalibrationStop();
    Costas_TPointCalibrationStop();
    mode = 0;
    mode_change_flag = 0;
}

// 模式 1/2 的 UI 参数以 kHz 保存，在驱动边界统一换算为 Hz。
static void AppState_ApplyOutputConfig(void) {
    if (mode == 1) {
        setup_AD9850_Hz((uint32_t) mode1_signal1_freq * 1000UL,
            (uint32_t) mode1_signal2_freq * 1000UL, 0, 0);
    } else if (mode == 2) {
        setup_AD9850_Hz((uint32_t) mode2_freq * 1000UL,
            (uint32_t) mode2_freq * 1000UL, 0, (unsigned char) mode2_phase_diff);
    }
}


void AppState_Task(void) {
    switch (mode) {
        case 1: case 2:
            if (mode_change_flag != 0) {
                AppState_ApplyOutputConfig();
                mode_change_flag = 0;
            }
            break;
        case 3:
            Costas_CalibrationTask();
            break;
        case 4: case 5: case 7:
            Costas_Task();
            break;
        case 6:
            Costas_TPointCalibrationTask();
            break;
        case 0:
            mode_change_flag = 0;
            break;
        default:
            AppState_Stop();
            break;
    }
}

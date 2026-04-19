#pragma once
#ifndef HWPWM_CALIB_esp32_H
#define HWPWM_CALIB_esp32_H

// clang-format off
static const Esp32HardwarePwm::CalibrationEntry hwpwmCalib_esp32[] = {
    // { frequency, resolution, overheadUs }
    { 13000, LEDC_TIMER_8_BIT,  1142 }, // 13kHz/ 8-bit
    { 14000, LEDC_TIMER_8_BIT,   541 }, // 14kHz/ 8-bit
    { 15000, LEDC_TIMER_8_BIT,  1449 }, // 15kHz/ 8-bit
    { 16000, LEDC_TIMER_8_BIT,  1481 }, // 16kHz/ 8-bit
};
// clang-format on

#define HWPWM_CALIB_TABLE hwpwmCalib_esp32
#define HWPWM_CALIB_COUNT (sizeof(hwpwmCalib_esp32)/sizeof(hwpwmCalib_esp32[0]))


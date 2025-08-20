/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * Esp32HardwarePWM.h
 *
 * Original Author: https://github.com/hrsavla
 * Esp32 version:   https://github.com/pljakobs
 *
 * This Esp32HardwarePWM library enables Sming framework users to use the ESP32 LEDC PWM API
 * 
 * The ESP32 PWM Hardware is much more powerful than the ESP8266, allowing wider PWM timers (up to 20 bit)
 * as well as much higher PWM frequencies (up to 40MHz for a 1 Bit wide PWM)
 * 
 * Features:
 * - Multiple PWM instances with independent configurations
 * - Automatic resource management (channels and timers)
 * - Support for high-speed and low-speed modes
 * - Configurable duty resolution (1-20 bits)
 * - Phase shifting for EMI reduction
 * - Hardware fade support
 * - Thread-safe operations
 *
 ****/

/** @defgroup   esp32_hw_pwm ESP32 Hardware PWM functions
 *  @brief      Provides ESP32 LEDC hardware pulse width modulation functions
 *  @{
*/

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <driver/ledc.h>
#include <hal/ledc_types.h>
#include <soc/soc_caps.h>
#include "Esp32PwmPlatform.h"
#include <array>

#define PWM_BAD_CHANNEL 0xff ///< Invalid PWM channel

/**
 * @brief PWM Channel information
 */
struct PwmChannelInfo {
    uint8_t gpio_pin = 0;                             ///< GPIO pin number
    ledc_channel_t channel = LEDC_CHANNEL_0;          ///< LEDC channel
    ledc_timer_t timer = LEDC_TIMER_0;                ///< Associated timer
    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;     ///< Speed mode
    uint32_t current_duty = 0;                        ///< Current duty cycle
    bool is_active = false;                           ///< Channel active status
};


/**
 * @brief ESP32 PWM Configuration parameters
 */

enum class PhaseShiftMode : uint8_t { OFF, AUTO, MANUAL };
enum class SpreadSpectrumMode : uint8_t { OFF, ON };

struct Esp32HwPwmPhaseShiftConfig {
    PhaseShiftMode mode = PhaseShiftMode::OFF;
    std::vector<int> manual_hpoints = {};
};

struct Esp32HwPwmModulationConfig {
    SpreadSpectrumMode mode = SpreadSpectrumMode::OFF;
    uint8_t WidthPercent = 0;
    uint8_t Subsampling = 0;
    uint8_t StepsizePercent = 0;
};

struct Esp32HwPwmTimerConfig {
    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer_bit_t resolution = LEDC_TIMER_10_BIT;
    ledc_timer_t timer_num = LEDC_TIMER_0;
    uint32_t frequency = 1000;
    ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;
};

struct Esp32HwPwmConfig {
    uint8_t channel_start = 0;
    Esp32HwPwmTimerConfig timer = {};
    Esp32HwPwmPhaseShiftConfig phase_shift = {};
    Esp32HwPwmModulationConfig modulation = {};
};

/**
 * @brief ESP32 Hardware PWM class
 * 
 * This class provides a C++ wrapper around the ESP32 LEDC PWM functionality.
 * 
 * Key features:
 * - Support for different frequencies and duty resolutions
 * - Phase shifting for EMI reduction
 * - Hardware fade support
 * - Thread-safe operations
 * - RAII resource management
 * 
 * @note This class is designed specifically for ESP32 architecture
 */
class Esp32HardwarePwm {
public:
    /**
     * @brief Construct PWM instance with default configuration
     * @param pins Vector of GPIO pins to control
     */
    Esp32HardwarePwm(std::vector<uint8_t>& pins);

    /**
     * @brief Construct PWM instance with custom configuration
     * @param pins Vector of GPIO pins to control
     * @param config PWM configuration parameters
     */
    Esp32HardwarePwm(std::vector<uint8_t>& pins, const Esp32HwPwmConfig& config);

    /**
     * @brief Destructor - automatically releases all allocated resources
     */
    virtual ~Esp32HardwarePwm();

    // Disable copy constructor and assignment operator to prevent resource conflicts
    Esp32HardwarePwm(const Esp32HardwarePwm&) = delete;
    Esp32HardwarePwm& operator=(const Esp32HardwarePwm&) = delete;


    /**
     * @brief Set PWM duty cycle for a specific pin
     * @param pin GPIO pin number
     * @param duty Duty cycle value (0 to getMaxDuty())
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
    bool setDuty(uint8_t pin, uint32_t duty, bool update_immediately = true);

    /**
     * @brief Get PWM duty cycle for a specific pin
     * @param pin GPIO pin number
     * @return Current duty cycle value
     */
    uint32_t getDuty(uint8_t pin) const;

    /**
     * @brief Set duty cycle as percentage (0.0 to 100.0)
     * @param pin GPIO pin number
     * @param percentage Duty cycle percentage
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
    bool setDutyPercent(uint8_t pin, float percentage, bool update_immediately = true);

    /**
     * @brief Get duty cycle as percentage
     * @param pin GPIO pin number
     * @return Duty cycle percentage (0.0 to 100.0)
     */
    float getDutyPercent(uint8_t pin) const;

    /**
     * @brief Arduino-style analogWrite function
     * @param pin GPIO pin number
     * @param duty Duty cycle value
     * @return true if successful, false otherwise
     */
    bool analogWrite(uint8_t pin, uint32_t duty) {
        return setDuty(pin, duty);
    }

    /**
     * @brief Change PWM frequency for all channels
     * @param frequency New frequency in Hz
     * @return true if successful, false otherwise
     * @note This affects all channels using the same timer
     */
    bool setFrequency(uint32_t frequency);

    /**
     * @brief Get current PWM frequency
     * @return Frequency in Hz
     */
    uint32_t getFrequency() const;

    /**
     * @brief Set PWM period in microseconds
     * @param period_us Period in microseconds
     * @return true if successful, false otherwise
     */
    bool setPeriod(uint32_t period_us);

    /**
     * @brief Get PWM period in microseconds
     * @return Period in microseconds
     */
    uint32_t getPeriod() const;

    /**
     * @brief Get maximum duty cycle value
     * @return Maximum duty value based on resolution
     */
    uint32_t getMaxDuty() const;

    /**
     * @brief Get duty resolution in bits
     * @return Resolution in bits (1-20)
     */
    uint8_t getResolution() const;

    /**
     * @brief Update all PWM outputs (apply pending changes)
     * @note Only needed when update_immediately was set to false
     */
    void update();

    /**
     * @brief Start PWM output on a specific pin
     * @param pin GPIO pin number
     * @return true if successful, false otherwise
     */
    bool start(uint8_t pin);

    /**
     * @brief Stop PWM output on a specific pin
     * @param pin GPIO pin number
     * @param idle_level Level to set pin when stopped (0 or 1)
     * @return true if successful, false otherwise
     */
    bool stop(uint8_t pin, uint8_t idle_level = 0);

    /**
     * @brief Start PWM output on all pins
     */
    void startAll();

    /**
     * @brief Stop PWM output on all pins
     * @param idle_level Level to set pins when stopped (0 or 1)
     */
    void stopAll(uint8_t idle_level = 0);

    /**
     * @brief Get channel info for a specific pin
     * @param pin GPIO pin number
     * @return Pointer to channel info, or nullptr if pin not found
     */
    const PwmChannelInfo* getChannelInfo(uint8_t pin) const;

    /**
     * @brief Get total number of configured channels
     * @return Number of channels
     */
    uint8_t getChannelCount() const;

    /**
     * @brief Check if PWM instance is properly initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;

    // Hardware fade functions (if enabled in config)

    /**
     * @brief Enable hardware fade functionality
     * @return true if successful, false otherwise
     * @note Must be called before using fade functions
     */
    bool enableFade();

    /**
     * @brief Disable hardware fade functionality
     */
    void disableFade();

    /**
     * @brief Start hardware fade to target duty
     * @param pin GPIO pin number
     * @param target_duty Target duty cycle
     * @param fade_time_ms Fade duration in milliseconds
     * @param wait_for_completion Block until fade completes (default: false)
     * @return true if successful, false otherwise
     */
    bool fadeToValue(uint8_t pin, uint32_t target_duty, uint32_t fade_time_ms, 
                     bool wait_for_completion = false);

    /**
     * @brief Start hardware fade to target percentage
     * @param pin GPIO pin number
     * @param target_percent Target duty cycle percentage
     * @param fade_time_ms Fade duration in milliseconds
     * @param wait_for_completion Block until fade completes (default: false)
     * @return true if successful, false otherwise
     */
    bool fadeToPercent(uint8_t pin, float target_percent, uint32_t fade_time_ms,
                       bool wait_for_completion = false);

private:
    std::vector<PwmChannelInfo> channels_;
    std::vector<uint8_t> pins_;
    size_t num_channels_ = 0;
    Esp32HwPwmConfig config_;
    bool initialized_;
    bool fade_installed_;
    uint8_t timer_num_;


    /**
     * @brief Initialize PWM instance
     * @param pins Array of GPIO pins
     * @param pin_count Number of pins
     * @return true if successful, false otherwise
     */
    bool initialize(const std::vector<uint8_t>& pins);

    /**
     * @brief Find channel index by pin number
     * @param pin GPIO pin number
     * @return Channel index, or -1 if not found
     */
    int findChannelIndex(uint8_t pin) const;

    /**
     * @brief Calculate hpoint for phase shifting
     * @param channel_index Index of the channel
     * @return Calculated hpoint value
     */
    int calculateHpoint(uint8_t channel_index) const;

    /**
     * @brief Apply duty cycle change to hardware
     * @param channel_info Channel information
     * @param duty New duty cycle
     * @param update_immediately Apply immediately
     * @return true if successful, false otherwise
     */
    bool applyDutyChange(const PwmChannelInfo& channel_info, uint32_t duty, 
                         bool update_immediately);
};

/** @} */

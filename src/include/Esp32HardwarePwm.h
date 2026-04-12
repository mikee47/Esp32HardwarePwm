/**
 * @file
 * @brief  ESP32 Hardware PWM (LEDC) driver
 * @author Peter Jakobs http://github.com/pljakobs
 *
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * This library wraps the ESP32 LEDC peripheral to provide hardware PWM with:
 * - Configurable duty resolution (1-20 bits) and frequency
 * - Multiple independent instances with distinct timer configurations
 * - Phase shifting (hpoint) for EMI/noise/power-spike reduction
 * - Spread spectrum modulation
 * - Hardware-accelerated linear fading
 */

/** @defgroup esp32_hw_pwm ESP32 Hardware PWM
 *  @brief    ESP32 LEDC hardware PWM driver
 *  @{
 */

#pragma once

#include <cstdint>
#include <vector>
#include <driver/ledc.h>
#include <soc/soc_caps.h>
#include <array>
#include <esp_attr.h>

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
    static constexpr uint8_t BadChannel = 0xff; ///< Invalid PWM channel indicator

    /**
     * @brief Defines PWM duty cycle percentage (0.0 = off, 100.0 = full on)
     */
    using DutyCycle = float;

    // -----------------------------------------------------------------------
    // Configuration types — declared before constructors so they are visible
    // in constructor parameter lists.
    // -----------------------------------------------------------------------

    enum class PhaseShiftMode : uint8_t {
        OFF,    ///< No phase shifting
        AUTO,   ///< Automatic phase shifting based on channel index
        MANUAL, ///< Manual phase shifting using provided hpoint values
    };

    enum class SpreadSpectrumMode : uint8_t {
        OFF, ///< Spread spectrum disabled
        ON,  ///< Spread spectrum enabled
    };

    struct PhaseShiftConfig {
        PhaseShiftMode mode = PhaseShiftMode::OFF;    ///< Phase shift mode
        std::vector<int> manual_hpoints = {};          ///< hpoint values for MANUAL mode, one per pin
    };

    struct SpreadSpectrumConfig {
        SpreadSpectrumMode mode = SpreadSpectrumMode::OFF; ///< Spread spectrum mode
        uint8_t WidthPercent = 0;   ///< Frequency deviation as percentage of base frequency
        uint16_t Subsampling = 0;   ///< Number of PWM cycles between frequency updates
    };

    struct TimerConfig {
        ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;    ///< LEDC speed mode
        ledc_timer_bit_t resolution = LEDC_TIMER_10_BIT; ///< Duty resolution in bits
        ledc_timer_t timer_num = LEDC_TIMER_0;           ///< LEDC timer index
        uint32_t frequency = 1000;                       ///< PWM frequency in Hz
        ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;          ///< Clock source
    };

    struct Config {
        ledc_channel_t channelStart = LEDC_CHANNEL_0; ///< First LEDC channel to allocate
        TimerConfig timer = {};                         ///< Timer configuration
        PhaseShiftConfig phaseShift = {};               ///< Phase shift configuration
        SpreadSpectrumConfig spreadSpectrum = {};        ///< Spread spectrum configuration
    };

    // -----------------------------------------------------------------------
    // Constructors / destructor
    // -----------------------------------------------------------------------

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
    Esp32HardwarePwm(std::vector<uint8_t>& pins, const Config& config);

    /**
     * @brief Destructor - automatically releases all allocated resources
     */
    ~Esp32HardwarePwm();

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
    bool setDuty(uint8_t pin, uint32_t duty, bool update_immediately = true) {
        int idx = getPinIndex(pin);
        if (idx < 0) return false;
        return setDutyChan((uint8_t)idx, duty, update_immediately);
    }

    /**
     * @brief Get PWM duty cycle for a specific pin
     * @param pin GPIO pin number
     * @return Current duty cycle value
     */
    uint32_t getDuty(uint8_t pin) {
        int idx = getPinIndex(pin);
        if (idx < 0) return 0;
        return getDutyChan((uint8_t)idx);
    }

    uint32_t getDutyChan(uint8_t channel);

    bool setDutyChan(uint8_t channel, uint32_t duty, bool update_immediately = true);

    /**
     * @brief Set duty cycle as percentage (0.0 to 100.0)
     * @param channel channel index 
     * @param percentage Duty cycle percentage
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
    bool setDutyChanPercent(uint8_t channel, DutyCycle percentage, bool update_immediately = true){
        if (percentage < 0.0f) percentage = 0.0f;
        if (percentage > 100.0f) percentage = 100.0f;

        uint32_t duty = static_cast<uint32_t>(percentage * getMaxDuty() / 100.0f);
        return setDutyChan(channel, duty, update_immediately);
    };

    /**
     * @brief Get duty cycle as percentage
     * @param channel channel index 
     * @return Duty cycle percentage (0.0 to 100.0)
     */
    DutyCycle getDutyChanPercent(uint8_t channel) {
        uint32_t duty = getDutyChan(channel);
        uint32_t max_duty = getMaxDuty();
        
        if (max_duty == 0) return 0.0f;
        
        return (static_cast<float>(duty) / max_duty) * 100.0f;
    };


    /**
     * @brief Arduino-style analogWrite function
     * @param pin GPIO pin number
     * @param duty Duty cycle value
     * @return true if successful, false otherwise
     */
    bool analogWrite(uint8_t pin, uint32_t duty) {
        return setDuty(pin, duty);
    }

    /** @brief change the phase shift for a given pin
     * @param pin GPIO pin number
     * @param phase_shift Phase shift value in points pwm resolution
     * @return true if successful, false otherwise
     */
    bool setPhaseShiftChan(uint8_t pin, uint32_t phase_shift, bool update_immediately=true) ;

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
     * @brief Update all PWM outputs (apply pending changes)auto pin : pins_
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
    bool stop(uint8_t pin, bool idle_level = false);

    /**
     * @brief Start PWM output on all pins
     */
    void startAll();

    /**
     * @brief Stop PWM output on all pins
     * @param idle_level Level to set pins when stopped (0 or 1)
     */
    void stopAll(bool idle_level = false);

    /**
     * @brief Get total number of configured pins
     * @return Number of pins
     */
    uint8_t getPinCount() const {
        return static_cast<uint8_t>(pins_.size());
    };

    /**
     * @brief Check if PWM instance is properly initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const {
        return initialized_;
    };

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

    /** Start hardware linear fade on a channel
     * @param channel_idx  0-based index into pins[] (same as setDutyChan)
     * @param target_duty  Target duty value (0 to getMaxDuty())
     * @param fade_time_ms Duration in milliseconds
     * @return true if started successfully
     */
    bool fadeToValueChan(uint8_t channel_idx, uint32_t target_duty, uint32_t fade_time_ms);

    /** Start hardware linear fade to a percentage
     * @param channel_idx  0-based index into pins[]
     * @param target_pct   Target duty as percentage (0.0–100.0)
     * @param fade_time_ms Duration in milliseconds
     * @return true if started successfully
     */
    bool fadeToPercentChan(uint8_t channel_idx, DutyCycle target_pct, uint32_t fade_time_ms);

    /** Returns true while a hardware fade is in progress on the given channel */
    bool isFadingChan(uint8_t channel_idx) const;

private:
    struct PinConfig {
        uint8_t gpioPin = 0;                          ///< GPIO pin number
        ledc_channel_t channel = LEDC_CHANNEL_0;      ///< LEDC hardware channel
        uint32_t currentDuty = 0;                     ///< Last duty value written
        int hpoint = 0;                               ///< Phase shift hpoint
        bool isActive = false;                        ///< True when channel is running
    };

    TimerConfig timer_;
    SpreadSpectrumConfig spreadSpectrum_;
    PhaseShiftConfig phaseShift_;
    std::vector<PinConfig> pins_;

 
    bool initialized_=false;
    bool fadeInstalled_=false;

    // Per-channel fade-done flags, set from LEDC fade callback (ISR-safe)
    std::array<volatile bool, SOC_LEDC_CHANNEL_NUM> fadeDone_{};

    /**
     * @brief Initialize PWM instance
     * @param pins Array of GPIO pins
     * @param pin_count Number of pins
     * @return true if successful, false otherwise
     */
    bool initialize();

    int getPinIndex(uint8_t gpioPin) const {
        for (size_t i = 0; i < pins_.size(); ++i) {
            if (pins_[i].gpioPin == gpioPin) return (int)i;
        }
        return -1;
    }

    /**
     * @brief Get channel info for a specific pin
     * @param pin GPIO pin number
     * @return Pointer to channel info, or nullptr if pin not found
     */
    PinConfig* getPinConfig(uint8_t gpioPin)  {
        for ( auto& pin : pins_) {
            if (pin.gpioPin == gpioPin) return &pin;
        }
        return nullptr;
    }

    /**
     * @brief Calculate hpoint for phase shifting
     * @param channel_index Index of the channel
     * @return Calculated hpoint value
     */
    int calculateHpoint(uint8_t channel_index) const {
        uint32_t max_duty = getMaxDuty();
        return (int)(max_duty * channel_index) / pins_.size();
    };

    /**
     * @brief Start spread spectrum modulation
     * @param frequency Center frequency in Hz
     * @param config Spread spectrum configuration
     * @return true if successful, false otherwise
     **/
    bool setupSpreadSpectrum(int frequency, SpreadSpectrumConfig& config);

    /**
     * @brief Handle spread spectrum modulation
     */
    IRAM_ATTR  void  handleSpreadSpectrum(); // placed in IRAM to reduce latency at kHz call rates

    // Fade callback registered with ledc_cb_register per channel
    static bool IRAM_ATTR fadeDoneCallback(const ledc_cb_param_t* param, void* arg);

    static IRAM_ATTR void timerIsr(void* arg); // placed in IRAM to reduce latency at kHz call rates
};


/** @} */

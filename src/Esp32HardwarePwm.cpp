#include "Esp32HardwarePwm.h"
#include <debug_progmem.h>
#include <driver/periph_ctrl.h>
#include <esp_err.h>
#include <algorithm>
#include <cassert>

/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * Esp32HardwarePWM.cpp
 *
 * Original Author: https://github.com/hrsavla
 * Esp32 version:   https://github.com/pljakobs
 *
 * This Esp32HardwarePWM library enables Sming framework users to use the ESP32 LEDC PWM API
 * 
 * The ESP32 PWM Hardware is much more powerful than the ESP8266, allowing wider PWM timers (up to 20 bit)
 * as well as much higher PWM frequencies (up to 40MHz for a 1 Bit wide PWM)
 * 
 * Reference: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/ledc.html
 * 
 * Key Features:
 * - Automatic resource management of LEDC channels and timers
 * - Support for multiple PWM instances with different configurations
 * - Thread-safe operations
 * - Hardware fade support
 * - Phase shifting for EMI reduction
 * - Compatible with both high-speed and low-speed modes
 *
 ****/

namespace {

/**
 * @brief Calculate maximum duty value for given resolution
 */
uint32_t maxDutyForResolution(ledc_timer_bit_t resolution) {
    return (1U << resolution) - 1;
}

/**
 * @brief Convert frequency to period in microseconds
 */
uint32_t frequencyToPeriod(uint32_t frequency) {
    return (frequency == 0) ? 0 : (1000000 / frequency);
}

/**
 * @brief Convert period in microseconds to frequency
 */
uint32_t periodToFrequency(uint32_t period_us) {
    return (period_us == 0) ? 0 : (1000000 / period_us);
}

} // anonymous namespace

/*
typedef struct {
    int gpio_num;                   //!< the LEDC output gpio_num, if you want to use gpio16, gpio_num = 16 
    ledc_mode_t speed_mode;         //!< LEDC speed speed_mode, high-speed mode (only exists on esp32) or low-speed mode 
    ledc_channel_t channel;         //!< LEDC channel (0 - LEDC_CHANNEL_MAX-1) 
    ledc_intr_type_t intr_type;     //!< configure interrupt, Fade interrupt enable  or Fade interrupt disable 
    ledc_timer_t timer_sel;         //!< Select the timer source of channel (0 - LEDC_TIMER_MAX-1) 
    uint32_t duty;                  //!< LEDC channel duty, the range of duty setting is [0, (2**duty_resolution)] 
    int hpoint;                     //!< LEDC channel hpoint value, the range is [0, (2**duty_resolution)-1] 
    struct {
        unsigned int output_invert: 1;//!< Enable (1) or disable (0) gpio output invert 
    } flags;                        //!< LEDC flags 

} ledc_channel_config_t;

typedef struct {
    ledc_mode_t speed_mode;                //!< LEDC speed speed_mode, high-speed mode (only exists on esp32) or low-speed mode */
    ledc_timer_bit_t duty_resolution;      //!< LEDC channel duty resolution */
    ledc_timer_t  timer_num;               //!< The timer source of channel (0 - LEDC_TIMER_MAX-1) */
    uint32_t freq_hz;                      //!< LEDC timer frequency (Hz) */
    ledc_clk_cfg_t clk_cfg;                //!< Configure LEDC source clock from ledc_clk_cfg_t.
                                           //   Note that LEDC_USE_RC_FAST_CLK and LEDC_USE_XTAL_CLK are
                                           //   non-timer-specific clock sources. You can not have one LEDC timer uses
                                           //   RC_FAST_CLK as the clock source and have another LEDC timer uses XTAL_CLK
                                           //   as its clock source. All chips except esp32 and esp32s2 do not have
                                           //   timer-specific clock sources, which means clock source for all timers
                                           //   must be the same one. */
    bool deconfigure;                      // Set this field to de-configure a LEDC timer which has been configured before
                                           //     Note that it will not check whether the timer wants to be de-configured
                                                is binded to any channel. Also, the timer has to be paused first before
                                                it can be de-configured.
                                                When this field is set, duty_resolution, freq_hz, clk_cfg fields are ignored. */
} ledc_timer_config_t;
*/

ledc_timer_config_t
//=============================================================================
// Esp32HardwarePwm Implementation
//=============================================================================

Esp32HardwarePwm::Esp32HardwarePwm(const uint8_t* pins, uint8_t pin_count)
    : initialized_(false), fade_installed_(false) {
    
    // Use default configuration
    Esp32PwmConfig default_config;
    config_ = default_config;
    
    initialized_ = initialize(pins, pin_count);
}

Esp32HardwarePwm::Esp32HardwarePwm(const uint8_t* pins, uint8_t pin_count, 
                                   const Esp32PwmConfig& config)
    : config_(config), initialized_(false), fade_installed_(false) {

    debug_i("Esp32HardwarePwm constructor called with parameters:");
    debug_i("  pin_count: %d", pin_count);
    debug_i("  pins: ");
    for (uint8_t i = 0; i < pin_count; ++i) {
        debug_i("    pins[%d]: %d", i, pins[i]);
    }
    debug_i("  config.frequency: %d", config.frequency);
    debug_i("  config.resolution: %d", config.resolution);
    debug_i("  config.speed_mode: %d", config.speed_mode);
    debug_i("  config.clock_source: %d", config.clock_source);
    debug_i("  config.use_phase_shift: %d", config.use_phase_shift);

    initialized_ = initialize(pins, pin_count);
}

Esp32HardwarePwm::~Esp32HardwarePwm() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (fade_installed_) {
        ledc_fade_func_uninstall();
    }
    
    if (initialized_) {
        // Stop all channels
        for (const auto& channel : channels_) {
            if (channel.is_active) {
                ledc_stop(channel.speed_mode, channel.channel, 0);
            }
        }
        
        // Release resources
        Esp32PwmResourceManager::getInstance().releaseChannels(channels_);
    }
}

bool Esp32HardwarePwm::initialize(const uint8_t* pins, uint8_t pin_count) {
    // should this be an assert or a runtime check?
    debug_i("Esp32HardwarePwm::initialize");
    if (pin_count == 0 || pin_count > SOC_LEDC_CHANNEL_NUM) {
        debug_i("Invalid pin count: %d", pin_count);
        return false;
    }
    
    // Enable LEDC peripheral
    periph_module_enable(PERIPH_LEDC_MODULE);
    
    // Allocate channels from resource manager
    debug_i("Esp32HardwarePwm::initialize - getting Channels");
    channels_ = Esp32PwmResourceManager::getInstance().allocateChannels(pins, pin_count, config_);
    if (channels_.empty()) {
        debug_e("Failed to allocate PWM channels");
        return false;
    }
    
    // List allocated channels for debugging
    
    // Configure each channel
    for (uint8_t i = 0;i<pin_count;i++) {
        ledc_channel_config_t channel_config = {
            .gpio_num = channels_[i].gpio_pin,
            .speed_mode = config_.speed_mode,
            .channel = channels_[i].channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = channels_[i].timer,
            .duty = 0,
            .hpoint = config_.use_phase_shift ? calculateHpoint(i) : (int)0
        };
        debug_i("Channel config: gpio_num=%d, speed_mode=%d, channel=%d, timer_sel=%d, duty=%d, hpoint=%d",
            channel_config.gpio_num, channel_config.speed_mode, channel_config.channel,
            channel_config.timer_sel, channel_config.duty, channel_config.hpoint);
        esp_err_t result = ledc_channel_config(&channel_config);
        if (result != ESP_OK) {
            debug_e("Failed to configure channel %d: %s", channels_[i].channel, esp_err_to_name(result));
            return false;
        }

        channels_[i].is_active = true;

        debug_i("Configured channel %d: pin=%d, timer=%d, hpoint=%d",
                channels_[i].channel, channels_[i].gpio_pin, channels_[i].timer, channel_config.hpoint);
    }
    
    debug_i("Allocated channels:");
    for (size_t i = 0; i < channels_.size(); ++i) {
        debug_i("  Channel %zu: pin=%d, channel=%d, timer=%d, speed_mode=%d",
                i, channels_[i].gpio_pin, channels_[i].channel, channels_[i].timer, channels_[i].speed_mode);
    }

    debug_i("Initialized PWM with %d channels", channels_.size());
    return true;
}

bool Esp32HardwarePwm::setDuty(uint8_t pin, uint32_t duty, bool update_immediately) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_) {
        debug_e("PWM not initialized");
        return false;
    }
    
    int channel_idx = findChannelIndex(pin);
    if (channel_idx < 0) {
        debug_e("Pin %d not found", pin);
        return false;
    }
    
    uint32_t max_duty = getMaxDuty();
    if (duty > max_duty) {
        debug_w("Duty %d exceeds maximum %d, clamping", duty, max_duty);
        duty = max_duty;
    }
    
    auto& channel = channels_[channel_idx];
    return applyDutyChange(channel, duty, update_immediately);
}

uint32_t Esp32HardwarePwm::getDuty(uint8_t pin) const {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_) {
        return 0;
    }
    
    int channel_idx = findChannelIndex(pin);
    if (channel_idx < 0) {
        return 0;
    }
    
    const auto& channel = channels_[channel_idx];
    return ledc_get_duty(channel.speed_mode, channel.channel);
}

bool Esp32HardwarePwm::setDutyPercent(uint8_t pin, float percentage, bool update_immediately) {
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;
    
    uint32_t duty = static_cast<uint32_t>((percentage / 100.0f) * getMaxDuty());
    return setDuty(pin, duty, update_immediately);
}

float Esp32HardwarePwm::getDutyPercent(uint8_t pin) const {
    uint32_t duty = getDuty(pin);
    uint32_t max_duty = getMaxDuty();
    
    if (max_duty == 0) return 0.0f;
    
    return (static_cast<float>(duty) / max_duty) * 100.0f;
}

bool Esp32HardwarePwm::setFrequency(uint32_t frequency) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_ || channels_.empty()) {
        return false;
    }
    
    // Update frequency for the timer used by our channels
    const auto& first_channel = channels_[0];
    esp_err_t result = ledc_set_freq(first_channel.speed_mode, first_channel.timer, frequency);
    
    if (result == ESP_OK) {
        config_.frequency = frequency;
        debug_i("Set frequency to %d Hz", frequency);
        return true;
    } else {
        debug_e("Failed to set frequency: %s", esp_err_to_name(result));
        return false;
    }
}

uint32_t Esp32HardwarePwm::getFrequency() const {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_ || channels_.empty()) {
        return 0;
    }
    
    const auto& first_channel = channels_[0];
    return ledc_get_freq(first_channel.speed_mode, first_channel.timer);
}

bool Esp32HardwarePwm::setPeriod(uint32_t period_us) {
    uint32_t frequency = periodToFrequency(period_us);
    return setFrequency(frequency);
}

uint32_t Esp32HardwarePwm::getPeriod() const {
    return frequencyToPeriod(getFrequency());
}

uint32_t Esp32HardwarePwm::getMaxDuty() const {
    return maxDutyForResolution(config_.resolution);
}

uint8_t Esp32HardwarePwm::getResolution() const {
    return static_cast<uint8_t>(config_.resolution);
}

void Esp32HardwarePwm::update() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // Update all channels
    for (const auto& channel : channels_) {
        if (channel.is_active) {
            ledc_update_duty(channel.speed_mode, channel.channel);
        }
    }
}

bool Esp32HardwarePwm::start(uint8_t pin) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    int channel_idx = findChannelIndex(pin);
    if (channel_idx < 0) {
        return false;
    }
    
    auto& channel = channels_[channel_idx];
    channel.is_active = true;
    return true;
}

bool Esp32HardwarePwm::stop(uint8_t pin, uint8_t idle_level) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    int channel_idx = findChannelIndex(pin);
    if (channel_idx < 0) {
        return false;
    }
    
    auto& channel = channels_[channel_idx];
    esp_err_t result = ledc_stop(channel.speed_mode, channel.channel, idle_level);
    
    if (result == ESP_OK) {
        channel.is_active = false;
        return true;
    }
    
    return false;
}

void Esp32HardwarePwm::startAll() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    for (auto& channel : channels_) {
        channel.is_active = true;
    }
}

void Esp32HardwarePwm::stopAll(uint8_t idle_level) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    for (auto& channel : channels_) {
        ledc_stop(channel.speed_mode, channel.channel, idle_level);
        channel.is_active = false;
    }
}

const PwmChannelInfo* Esp32HardwarePwm::getChannelInfo(uint8_t pin) const {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    int channel_idx = findChannelIndex(pin);
    if (channel_idx < 0) {
        return nullptr;
    }
    
    return &channels_[channel_idx];
}

uint8_t Esp32HardwarePwm::getChannelCount() const {
    return static_cast<uint8_t>(channels_.size());
}

bool Esp32HardwarePwm::isInitialized() const {
    return initialized_;
}

bool Esp32HardwarePwm::enableFade() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (fade_installed_) {
        return true;
    }
    
    esp_err_t result = ledc_fade_func_install(0);
    if (result == ESP_OK) {
        fade_installed_ = true;
        debug_i("Fade functionality enabled");
        return true;
    } else {
        debug_e("Failed to enable fade: %s", esp_err_to_name(result));
        return false;
    }
}

void Esp32HardwarePwm::disableFade() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (fade_installed_) {
        ledc_fade_func_uninstall();
        fade_installed_ = false;
        debug_i("Fade functionality disabled");
    }
}

bool Esp32HardwarePwm::fadeToValue(uint8_t pin, uint32_t target_duty, uint32_t fade_time_ms, 
                                   bool wait_for_completion) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!initialized_ || !fade_installed_) {
        debug_e("PWM not initialized or fade not enabled");
        return false;
    }
    
    int channel_idx = findChannelIndex(pin);
    if (channel_idx < 0) {
        debug_e("Pin %d not found", pin);
        return false;
    }
    
    uint32_t max_duty = getMaxDuty();
    if (target_duty > max_duty) {
        target_duty = max_duty;
    }
    
    const auto& channel = channels_[channel_idx];
    
    esp_err_t result = ledc_set_fade_time_and_start(
        channel.speed_mode, 
        channel.channel, 
        target_duty, 
        fade_time_ms,
        wait_for_completion ? LEDC_FADE_WAIT_DONE : LEDC_FADE_NO_WAIT
    );
    
    if (result == ESP_OK) {
        debug_i("Started fade on pin %d to duty %d over %d ms", pin, target_duty, fade_time_ms);
        return true;
    } else {
        debug_e("Failed to start fade: %s", esp_err_to_name(result));
        return false;
    }
}

bool Esp32HardwarePwm::fadeToPercent(uint8_t pin, float target_percent, uint32_t fade_time_ms,
                                     bool wait_for_completion) {
    if (target_percent < 0.0f) target_percent = 0.0f;
    if (target_percent > 100.0f) target_percent = 100.0f;
    
    uint32_t target_duty = static_cast<uint32_t>((target_percent / 100.0f) * getMaxDuty());
    return fadeToValue(pin, target_duty, fade_time_ms, wait_for_completion);
}

int Esp32HardwarePwm::findChannelIndex(uint8_t pin) const {
    for (size_t i = 0; i < channels_.size(); i++) {
        if (channels_[i].gpio_pin == pin) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Esp32HardwarePwm::calculateHpoint(uint8_t channel_index) const {
    if (!config_.use_phase_shift || channels_.empty()) {
        return 0;
    }
    
    uint32_t max_duty = getMaxDuty();
    return (int)(max_duty * channel_index) / channels_.size();
}

bool Esp32HardwarePwm::applyDutyChange(const PwmChannelInfo& channel_info, uint32_t duty, 
                                       bool update_immediately) {
    esp_err_t result;
    
    if (config_.use_phase_shift) {
        result = ledc_set_duty_with_hpoint(channel_info.speed_mode, channel_info.channel, 
                                          duty, calculateHpoint(&channel_info - &channels_[0]));
    } else {
        result = ledc_set_duty(channel_info.speed_mode, channel_info.channel, duty);
    }
    
    if (result != ESP_OK) {
        debug_e("Failed to set duty: %s", esp_err_to_name(result));
        return false;
    }
    
    if (update_immediately) {
        result = ledc_update_duty(channel_info.speed_mode, channel_info.channel);
        if (result != ESP_OK) {
            debug_e("Failed to update duty: %s", esp_err_to_name(result));
            return false;
        }
    }
    
    return true;
}

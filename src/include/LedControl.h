/**
 * LedControl.h - ESP32 LEDC Control Library for Sming
 * 
 * This library provides low-level access to the ESP32 LEDC peripheral
 * following Sming's philosophy of staying "close to the metal".
 * 
 * Features:
 * - Zero FreeRTOS dependencies
 * - Minimal overhead with inline functions
 * - Automatic resource management
 * - Type-safe timer classes
 * - Clean modern C++ API
 * 
 * Author: Based on work by pljakobs, restructured per mikee47 feedback
 * License: LGPL v3
 */

#pragma once

#include <driver/ledc.h>
#include <soc/soc_caps.h>
#include <cstdint>
#include <algorithm>

namespace LEDC {

// Platform-specific capabilities
#if CONFIG_IDF_TARGET_ESP32
    constexpr uint8_t MAX_LS_CHANNELS = 8;
    constexpr uint8_t MAX_HS_CHANNELS = 8;
    constexpr bool HAS_HIGH_SPEED = true;
#elif CONFIG_IDF_TARGET_ESP32C3
    constexpr uint8_t MAX_LS_CHANNELS = 6;
    constexpr uint8_t MAX_HS_CHANNELS = 0;
    constexpr bool HAS_HIGH_SPEED = false;
#else
    constexpr uint8_t MAX_LS_CHANNELS = 8;
    constexpr uint8_t MAX_HS_CHANNELS = 0;
    constexpr bool HAS_HIGH_SPEED = false;
#endif

constexpr uint8_t MAX_TIMERS_PER_MODE = 4;

/**
 * Resource allocation tracker
 * Single instance manages all timer and channel allocation
 */
class ResourceManager {
public:
    static ResourceManager& instance() {
        static ResourceManager mgr;
        return mgr;
    }
    
    int8_t allocateTimer(ledc_mode_t mode) {
        auto& mask = (mode == LEDC_LOW_SPEED_MODE) ? lsTimerMask : hsTimerMask;
        for (uint8_t i = 0; i < MAX_TIMERS_PER_MODE; ++i) {
            if (!(mask & (1 << i))) {
                mask |= (1 << i);
                return i;
            }
        }
        return -1; // No free timer
    }
    
    void freeTimer(ledc_mode_t mode, uint8_t timer) {
        auto& mask = (mode == LEDC_LOW_SPEED_MODE) ? lsTimerMask : hsTimerMask;
        if (timer < MAX_TIMERS_PER_MODE) {
            mask &= ~(1 << timer);
        }
    }
    
    int8_t allocateChannel(ledc_mode_t mode) {
        auto& mask = (mode == LEDC_LOW_SPEED_MODE) ? lsChannelMask : hsChannelMask;
        uint8_t maxChannels = (mode == LEDC_LOW_SPEED_MODE) ? MAX_LS_CHANNELS : MAX_HS_CHANNELS;
        
        for (uint8_t i = 0; i < maxChannels; ++i) {
            if (!(mask & (1 << i))) {
                mask |= (1 << i);
                return i;
            }
        }
        return -1; // No free channel
    }
    
    void freeChannel(ledc_mode_t mode, uint8_t channel) {
        auto& mask = (mode == LEDC_LOW_SPEED_MODE) ? lsChannelMask : hsChannelMask;
        uint8_t maxChannels = (mode == LEDC_LOW_SPEED_MODE) ? MAX_LS_CHANNELS : MAX_HS_CHANNELS;
        
        if (channel < maxChannels) {
            mask &= ~(1 << channel);
        }
    }
    
    /**
     * Get number of allocated timers for a mode
     */
    uint8_t getAllocatedTimerCount(ledc_mode_t mode) const {
        auto mask = (mode == LEDC_LOW_SPEED_MODE) ? lsTimerMask : hsTimerMask;
        uint8_t count = 0;
        for (uint8_t i = 0; i < MAX_TIMERS_PER_MODE; ++i) {
            if (mask & (1 << i)) count++;
        }
        return count;
    }
    
    /**
     * Get number of allocated channels for a mode
     */
    uint8_t getAllocatedChannelCount(ledc_mode_t mode) const {
        auto mask = (mode == LEDC_LOW_SPEED_MODE) ? lsChannelMask : hsChannelMask;
        uint8_t maxChannels = (mode == LEDC_LOW_SPEED_MODE) ? MAX_LS_CHANNELS : MAX_HS_CHANNELS;
        uint8_t count = 0;
        for (uint8_t i = 0; i < maxChannels; ++i) {
            if (mask & (1 << i)) count++;
        }
        return count;
    }

private:
    ResourceManager() = default;
    ~ResourceManager() = default;
    
    // Disable copy and move operations for singleton
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;
    
    uint8_t lsTimerMask = 0;
    uint8_t hsTimerMask = 0;
    uint16_t lsChannelMask = 0;
    uint16_t hsChannelMask = 0;
};

/**
 * Base timer class
 */
class TimerBase {
public:
    TimerBase(ledc_mode_t mode, uint32_t frequency, ledc_timer_bit_t resolution, 
              ledc_clk_cfg_t clockSource = LEDC_AUTO_CLK)
        : mode_(mode), frequency_(frequency), resolution_(resolution), clockSource_(clockSource) {
        
        timerNum_ = ResourceManager::instance().allocateTimer(mode);
        if (timerNum_ >= 0) {
            valid_ = configure();
        }
    }
    
    ~TimerBase() {
        if (valid_ && timerNum_ >= 0) {
            ResourceManager::instance().freeTimer(mode_, timerNum_);
        }
    }
    
    // Non-copyable, moveable
    TimerBase(const TimerBase&) = delete;
    TimerBase& operator=(const TimerBase&) = delete;
    TimerBase(TimerBase&& other) noexcept = default;
    TimerBase& operator=(TimerBase&& other) noexcept = default;
    
    bool setFrequency(uint32_t frequency) {
        if (!valid_) return false;
        
        frequency_ = frequency;
        
        ledc_timer_config_t config = {
            .speed_mode = mode_,
            .duty_resolution = resolution_,
            .timer_num = static_cast<ledc_timer_t>(timerNum_),
            .freq_hz = frequency_,
            .clk_cfg = clockSource_
        };
        
        return ledc_timer_config(&config) == ESP_OK;
    }
    
    uint32_t getFrequency() const { return frequency_; }
    
    uint32_t getMaxDuty() const { 
        return (1U << resolution_) - 1; 
    }
    
    ledc_timer_bit_t getResolution() const { return resolution_; }
    ledc_mode_t getMode() const { return mode_; }
    ledc_timer_t getTimerNum() const { return static_cast<ledc_timer_t>(timerNum_); }
    
    operator bool() const { return valid_; }

private:
    bool configure() {
        ledc_timer_config_t config = {
            .speed_mode = mode_,
            .duty_resolution = resolution_,
            .timer_num = static_cast<ledc_timer_t>(timerNum_),
            .freq_hz = frequency_,
            .clk_cfg = clockSource_
        };
        
        return ledc_timer_config(&config) == ESP_OK;
    }
    
    ledc_mode_t mode_;
    uint32_t frequency_;
    ledc_timer_bit_t resolution_;
    ledc_clk_cfg_t clockSource_;
    int8_t timerNum_ = -1;
    bool valid_ = false;
};

/**
 * Low-speed timer class
 */
class LSTimer : public TimerBase {
public:
    explicit LSTimer(uint32_t frequency = 1000, 
                    ledc_timer_bit_t resolution = LEDC_TIMER_10_BIT,
                    ledc_clk_cfg_t clockSource = LEDC_AUTO_CLK)
        : TimerBase(LEDC_LOW_SPEED_MODE, frequency, resolution, clockSource) {}
};

/**
 * High-speed timer class (ESP32 only)
 */
class HSTimer : public TimerBase {
public:
    explicit HSTimer(uint32_t frequency = 1000,
                    ledc_timer_bit_t resolution = LEDC_TIMER_10_BIT,
                    ledc_clk_cfg_t clockSource = LEDC_AUTO_CLK)
        : TimerBase(LEDC_HIGH_SPEED_MODE, frequency, resolution, clockSource) {
        
        static_assert(HAS_HIGH_SPEED, "High-speed mode not available on this SoC");
    }
};

/**
 * LEDC Channel class
 */
class Channel {
public:
    struct Config {
        uint8_t gpio;
        uint32_t duty = 0;
        uint32_t hpoint = 0;
    };
    
    explicit Channel(TimerBase& timer) : timer_(timer) {
        if (timer_) {
            channelNum_ = ResourceManager::instance().allocateChannel(timer_.getMode());
            valid_ = (channelNum_ >= 0);
        }
    }
    
    ~Channel() {
        if (valid_ && channelNum_ >= 0) {
            stop(0);
            ResourceManager::instance().freeChannel(timer_.getMode(), channelNum_);
        }
    }
    
    // Non-copyable, moveable
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&& other) noexcept = default;
    Channel& operator=(Channel&& other) noexcept = default;
    
    bool configure(const Config& config) {
        if (!valid_ || !timer_) return false;
        
        gpio_ = config.gpio;
        
        ledc_channel_config_t channelConfig = {
            .gpio_num = config.gpio,
            .speed_mode = timer_.getMode(),
            .channel = static_cast<ledc_channel_t>(channelNum_),
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = timer_.getTimerNum(),
            .duty = config.duty,
            .hpoint = config.hpoint
        };
        
        bool result = ledc_channel_config(&channelConfig) == ESP_OK;
        if (result) {
            configured_ = true;
            currentDuty_ = config.duty;
        }
        return result;
    }
    
    bool setDuty(uint32_t duty) {
        if (!configured_) return false;
        
        duty = std::min(duty, timer_.getMaxDuty());
        
        esp_err_t err = ledc_set_duty(timer_.getMode(), 
                                     static_cast<ledc_channel_t>(channelNum_), 
                                     duty);
        if (err == ESP_OK) {
            err = ledc_update_duty(timer_.getMode(), 
                                  static_cast<ledc_channel_t>(channelNum_));
            if (err == ESP_OK) {
                currentDuty_ = duty;
                return true;
            }
        }
        return false;
    }
    
    uint32_t getDuty() const { return currentDuty_; }
    
    bool setDutyPercent(float percent) {
        percent = std::max(0.0f, std::min(100.0f, percent));
        uint32_t duty = static_cast<uint32_t>((percent / 100.0f) * timer_.getMaxDuty());
        return setDuty(duty);
    }
    
    float getDutyPercent() const {
        if (timer_.getMaxDuty() == 0) return 0.0f;
        return (static_cast<float>(currentDuty_) / timer_.getMaxDuty()) * 100.0f;
    }
    
    bool start() {
        if (!configured_) return false;
        return setDuty(currentDuty_);
    }
    
    bool stop(uint8_t idleLevel = 0) {
        if (!configured_) return false;
        
        esp_err_t err = ledc_stop(timer_.getMode(), 
                                 static_cast<ledc_channel_t>(channelNum_), 
                                 idleLevel);
        return err == ESP_OK;
    }
    
    // Arduino-style compatibility
    bool analogWrite(uint32_t duty) { return setDuty(duty); }
    
    uint8_t getGpio() const { return gpio_; }
    ledc_channel_t getChannelNum() const { return static_cast<ledc_channel_t>(channelNum_); }
    
    operator bool() const { return valid_ && configured_; }

private:
    TimerBase& timer_;
    int8_t channelNum_ = -1;
    uint8_t gpio_ = 0;
    uint32_t currentDuty_ = 0;
    bool valid_ = false;
    bool configured_ = false;
};

/**
 * Utility functions
 */
namespace Utils {
    /**
     * Convert servo angle (0-180°) to duty cycle
     */
    inline uint32_t servoAngleToDuty(float angle, const TimerBase& timer) {
        angle = std::max(0.0f, std::min(180.0f, angle));
        
        // Standard servo: 1-2ms pulse width, 20ms period
        uint16_t pulseWidthUs = 1000 + static_cast<uint16_t>((angle / 180.0f) * 1000);
        
        // Calculate duty cycle based on timer frequency and resolution
        uint32_t periodUs = 1000000 / timer.getFrequency();
        return (static_cast<uint64_t>(pulseWidthUs) * timer.getMaxDuty()) / periodUs;
    }
    
    /**
     * Convert RGB values to duty cycles
     */
    inline void rgbToDuty(uint8_t r, uint8_t g, uint8_t b, const TimerBase& timer,
                         uint32_t& redDuty, uint32_t& greenDuty, uint32_t& blueDuty) {
        uint32_t maxDuty = timer.getMaxDuty();
        redDuty = (static_cast<uint32_t>(r) * maxDuty) / 255;
        greenDuty = (static_cast<uint32_t>(g) * maxDuty) / 255;
        blueDuty = (static_cast<uint32_t>(b) * maxDuty) / 255;
    }
    
    /**
     * HSV to RGB conversion
     */
    inline void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
        h = fmod(h, 360.0f);
        s = std::max(0.0f, std::min(1.0f, s));
        v = std::max(0.0f, std::min(1.0f, v));
        
        float c = v * s;
        float x = c * (1.0f - abs(fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float rf, gf, bf;
        
        if (h >= 0 && h < 60) {
            rf = c; gf = x; bf = 0;
        } else if (h >= 60 && h < 120) {
            rf = x; gf = c; bf = 0;
        } else if (h >= 120 && h < 180) {
            rf = 0; gf = c; bf = x;
        } else if (h >= 180 && h < 240) {
            rf = 0; gf = x; bf = c;
        } else if (h >= 240 && h < 300) {
            rf = x; gf = 0; bf = c;
        } else {
            rf = c; gf = 0; bf = x;
        }
        
        r = static_cast<uint8_t>((rf + m) * 255);
        g = static_cast<uint8_t>((gf + m) * 255);
        b = static_cast<uint8_t>((bf + m) * 255);
    }
}

} // namespace LEDC

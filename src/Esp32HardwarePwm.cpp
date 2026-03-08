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
    ledc_mode_t speed_mode;                //!< LEDC speed speed_mode, high-speed mode (only exists on esp32) or low-speed mode 
    ledc_timer_bit_t duty_resolution;      //!< LEDC channel duty resolution 
    ledc_timer_t  timer_num;               //!< The timer source of channel (0 - LEDC_TIMER_MAX-1) 
    uint32_t freq_hz;                      //!< LEDC timer frequency (Hz) 
    ledc_clk_cfg_t clk_cfg;                //!< Configure LEDC source clock from ledc_clk_cfg_t.
                                           //   Note that LEDC_USE_RC_FAST_CLK and LEDC_USE_XTAL_CLK are
                            /home/pjakobs/devel/esp_rgbww_firmware/Components/Esp32HardwarePwm/src/Esp32HardwarePwm.cpp               //   non-timer-specific clock sources. You can not have one LEDC timer uses
                                           //   RC_FAST_CLK as the clock source and have another LEDC timer uses XTAL_CLK
                                           //   as its clock source. All chips except esp32 and esp32s2 do not have
                                           //   timer-specific clock sources, which means clock source for all timers
                                           //   must be the same one. 
    bool deconfigure;                      //   Set this field to de-configure a LEDC timer which has been configured before
                                           //   Note that it will not check whether the timer wants to be de-configured
                                           //   is binded to any channel. Also, the timer has to be paused first before
                                           //   it can be de-configured.
                                           //   When this field is set, duty_resolution, freq_hz, clk_cfg fields are ignored. 
} ledc_timer_t;
*/




// static_assert(ChannelStart + pins.size() <= CHANNEL_MAX, "Channel range exceeds available channels!");
//=============================================================================
// Esp32HardwarePwm Implementation
//=============================================================================

Esp32HardwarePwm::Esp32HardwarePwm(std::vector<uint8_t>& pins)
    : Esp32HardwarePwm(pins, Esp32HwPwmConfig{}) {
}

Esp32HardwarePwm::Esp32HardwarePwm(std::vector<uint8_t>& pins, const Esp32HwPwmConfig& config)
    : initialized_(false), fadeInstalled_(false) {
        
    timer_=config.timer;
    spreadSpectrum_=config.spreadSpectrum;
    phaseShift_=config.phaseShift;
    pins_.resize(pins.size());

    // basic sanity checks
    if(pins.size() <= 0) {
        debug_e("Pin count must be positive");
        return ;
    }
    if(pins.size() >= SOC_LEDC_CHANNEL_NUM) {
        debug_e("Pin count exceeds available LEDC channels");
        return ;
    }
    if(pins.size() + config.channelStart > SOC_LEDC_CHANNEL_NUM) {
        debug_e("Channel range exceeds available channels");
        return ;
    }

    /* populate the pins_ array
        gpioPin is assigned the pin passed in the pins array
        channel is counted up from config.channelStart
        hpoint is handled according to config.phaseShift.mode:
            for AUTO, the hpoint is calculated based on the pin index
            for MANUAL, the hpoint is taken from the provided manual_hpoints array
                if the number of provided hpoints is less than the number of pins, set the remaining hpoints to 0
                if the number of provided hpoints is more than the number of pins, the extra hpoints will be ignored
    */

    for (uint8_t i = 0; i < pins.size(); ++i) {
        pins_.at(i).gpioPin = pins.at(i);
        pins_.at(i).channel = (ledc_channel_t)(config.channelStart + i);

        switch (phaseShift_.mode)
        {
        case PhaseShiftMode::AUTO:
            pins_.at(i).hpoint = calculateHpoint(i);
            break;
        case PhaseShiftMode::MANUAL:
            pins_.at(i).hpoint = (i < config.phaseShift.manual_hpoints.size())
                ? config.phaseShift.manual_hpoints.at(i)
                : 0;
            break;
        case PhaseShiftMode::OFF:
        default:
            pins_.at(i).hpoint = 0;
            break;
        }
    }


    debug_i("PWM Constructor Configuration:");
    debug_i("  Timer: num=%d, resolution=%d, freq=%d, speed_mode=%d, clk_cfg=%d",
        timer_.timer_num,
        timer_.resolution,
        timer_.frequency,
        timer_.speed_mode,
        timer_.clk_cfg);
    debug_i("  SpreadSpectrum: mode=%d, WidthPercent=%d, Subsampling=%d",
        spreadSpectrum_.mode,
        spreadSpectrum_.WidthPercent,
        spreadSpectrum_.Subsampling);
    debug_i("  PhaseShiftMode: %d", static_cast<int>(phaseShift_.mode));
    debug_i("  Channel start: %d", config.channelStart);
    debug_i("  Pins : %i", pins.size());
    for (size_t i = 0; i < pins.size(); ++i) {
        debug_i("    Pin[%i]: %d, hpoint: %d", i, pins[i], pins_.at(i).hpoint);
    }
    
    initialized_ = initialize();
}

Esp32HardwarePwm::~Esp32HardwarePwm() {
    
    if (fadeInstalled_) {
        ledc_fade_func_uninstall();
    }
    
    if (initialized_) {
        // Stop all channels
        for (const auto& pin : pins_) {
            if (pin.isActive) {
                ledc_stop(timer_.speed_mode, pin.channel, 0);
            }
        }

        ledc_timer_config_t timer_config = {
            .timer_num = timer_.timer_num,
            .deconfigure = true
        };
        auto result = ledc_timer_config(&timer_config);
    }
}

bool Esp32HardwarePwm::initialize() {
    debug_i("Esp32HardwarePwm::initialize");

    // Enable LEDC peripheral
    periph_module_enable(PERIPH_LEDC_MODULE);

    debug_i("initialize timer");
    // initialize the timer
    ledc_timer_config_t timer_config = {
        .speed_mode = timer_.speed_mode,
        .duty_resolution = timer_.resolution,
        .timer_num = timer_.timer_num,
        .freq_hz = timer_.frequency,
        .clk_cfg = timer_.clk_cfg
    };

    auto result=ledc_timer_config(&timer_config);
    if (result != ESP_OK) {
        debug_e("Failed to configure timer: %s", esp_err_to_name(result));
        return false;
    }

    // Allocate channels from resource manager
    debug_i("Esp32HardwarePwm::initialize - getting Channels");

    if (spreadSpectrum_.mode != SpreadSpectrumMode::OFF) {
        // Configure spread spectrum
        debug_i("Configuring spread spectrum");
        setupSpreadSpectrum(timer_.frequency, &spreadSpectrum_);
    }

    // Configure each channel
    for (size_t i = 0; i < pins_.size(); ++i) {
        ledc_channel_config_t channel_config = {
            .gpio_num = pins_.at(i).gpioPin,
            .speed_mode = timer_.speed_mode,
            .channel = pins_.at(i).channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = timer_.timer_num,
            .duty = 0,
            .hpoint = pins_.at(i).hpoint
        };
        debug_i("Channel config: gpio_num=%d, speed_mode=%d, channel=%d, timer_sel=%d, duty=%d, hpoint=%d",
            channel_config.gpio_num, channel_config.speed_mode, channel_config.channel,
            channel_config.timer_sel, channel_config.duty, channel_config.hpoint);

        auto result = ledc_channel_config(&channel_config);
        if (result != ESP_OK) {
            debug_e("Failed to configure pin %d: %s", channel_config.gpio_num, esp_err_to_name(result));
            return false;
        }

        pins_.at(i).isActive = true;

        debug_i("configured pin %d:\n  channel: %d\n  hpoint: %d",
            pins_.at(i).gpioPin, pins_.at(i).channel, pins_.at(i).hpoint);
    }
    
    debug_i("Initialized PWM with %d pins", pins_.size());
    return true;
}

uint32_t Esp32HardwarePwm::getDutyChan(uint8_t channel) {
    if (!initialized_) {
        return 0;
    }
    //debug_i("Getting duty for channel %d", channel);
    if (channel >= pins_.size()) {
        return 0;
    }
    return ledc_get_duty(timer_.speed_mode, pins_.at(channel).channel);
}

bool Esp32HardwarePwm::setDutyChan(uint8_t channel, uint32_t duty, bool update_immediately) {
    if (!initialized_||channel >= pins_.size()) {
        return false;
    }
    
    if (pins_.at(channel).currentDuty==duty) {
        //debug_i("No change in duty for pin %d channel %d: %d", pins_.at(channel).gpioPin,pins_.at(channel).channel,duty);
        return true; // no change
    }

    uint32_t max_duty = getMaxDuty();
    if (duty > max_duty) {
        debug_w("Duty %d exceeds maximum %d, clamping", duty, max_duty);
        duty = max_duty;
    }

    debug_i("Setting duty for pin %d channel %d: %d", pins_.at(channel).gpioPin,pins_.at(channel).channel,duty);
    pins_.at(channel).currentDuty = duty;
    ledc_set_duty(timer_.speed_mode, pins_.at(channel).channel, duty);

    if (update_immediately) {
        ledc_update_duty(timer_.speed_mode, pins_.at(channel).channel);
    }
    return true;
}

bool Esp32HardwarePwm::setPhaseShiftChan(uint8_t channel, uint32_t phase_shift, bool update_immediately) {
    if (!initialized_) {
        return false;
    }

    pins_.at(channel).hpoint = phase_shift;

    ledc_set_duty_with_hpoint(timer_.speed_mode, pins_.at(channel).channel, pins_.at(channel).currentDuty, pins_.at(channel).hpoint);
    if (update_immediately) {
        ledc_update_duty(timer_.speed_mode, pins_.at(channel).channel);
    }
    return true;
}

bool Esp32HardwarePwm::setFrequency(uint32_t frequency) {
    
    if (!initialized_ || pins_.empty()) {
        return false;
    }
    
    // Update frequency for the timer used by our channels
      
    esp_err_t result = ledc_set_freq(timer_.speed_mode, timer_.timer_num, frequency);
    
    if (result == ESP_OK) {
        timer_.frequency = frequency;
        // debug_i("Set frequency to %d Hz", frequency);
        return true;
    } else {
        debug_e("Failed to set frequency: %s", esp_err_to_name(result));
        return false;
    }
}

uint32_t Esp32HardwarePwm::getFrequency() const {

    if (!initialized_) {
        return 0;
    }

    return ledc_get_freq(timer_.speed_mode, timer_.timer_num);
}

bool Esp32HardwarePwm::setPeriod(uint32_t period_us) {
    uint32_t frequency = periodToFrequency(period_us);
    return setFrequency(frequency);
}

uint32_t Esp32HardwarePwm::getPeriod() const {
    return frequencyToPeriod(getFrequency());
}

uint32_t Esp32HardwarePwm::getMaxDuty() const {
    return maxDutyForResolution(timer_.resolution);
}

uint8_t Esp32HardwarePwm::getResolution() const {
    return static_cast<uint8_t>(timer_.resolution);
}

void Esp32HardwarePwm::update() {
    // todo: this does not do anything meaningful
    if (!initialized_) {
        return;
    }
    
    // Update all channels
    for (const auto& pin : pins_) {
        if (pin.isActive) {
            ledc_update_duty(timer_.speed_mode, pin.channel);
        }
    }
}

bool Esp32HardwarePwm::start(uint8_t pin) {
    if (!initialized_) {
        return false;
    }
    return true;
// todo: do something useful
}

bool Esp32HardwarePwm::stop(uint8_t pin, uint8_t idle_level) {
    if (!initialized_) {
        return false;
    }

    auto pin_config = getPinConfig(pin);
    if(!pin_config) {
        debug_e("Pin %d not found", pin);
        return false;
    }
    esp_err_t result = ledc_stop(timer_.speed_mode, pin_config->channel, idle_level);
    
    if (result == ESP_OK) {
        pin_config->isActive = false;
        return true;
    }
    
    return false;
}

void Esp32HardwarePwm::startAll() {

// todo: do something useful
}

void Esp32HardwarePwm::stopAll(uint8_t idle_level) {


    for (auto& pin : pins_) {
        ledc_stop(timer_.speed_mode, pin.channel, idle_level);        
        pin.isActive = false;
    }
}


bool Esp32HardwarePwm::enableFade() {
    
    if (fadeInstalled_) {
        return true;
    }
    
    esp_err_t result = ledc_fade_func_install(0); // enable hardware fade with no interrupt config
    if (result == ESP_OK) {
        fadeInstalled_ = true;
        debug_i("Fade functionality enabled");
        return true;
    } else {
        debug_e("Failed to enable fade: %s", esp_err_to_name(result));
        return false;
    }
}

void Esp32HardwarePwm::disableFade() {
    
    if (fadeInstalled_) {
        ledc_fade_func_uninstall(); // disable hardware fade
        fadeInstalled_ = false;
        debug_i("Fade functionality disabled");
    }
}

bool Esp32HardwarePwm::fadeToValue(uint8_t pin, uint32_t target_duty, uint32_t fade_time_ms, 
                                   bool wait_for_completion) {
       
    if (!initialized_ || !fadeInstalled_) {
        debug_e("PWM not initialized or fade not enabled");
        return false;
    }

    auto pin_config = getPinConfig(pin);
    if(!pin_config) {
        debug_e("Pin %d not found", pin);
        return false;
    }
    uint32_t max_duty = getMaxDuty();
    if (target_duty > max_duty) {
        target_duty = max_duty;
    }
    
    esp_err_t result = ledc_set_fade_time_and_start(
        timer_.speed_mode,
        pin_config->channel,
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

bool Esp32HardwarePwm::setupSpreadSpectrum(int frequency, Esp32HwPwmSpreadSpectrumConfig* config) {
   // Validate and apply spread spectrum configuration
    if (config) {
        spreadSpectrum_ = *config;
    }
    int interval_us = 1000000 * spreadSpectrum_.Subsampling / frequency;
    esp_timer_create_args_t timer_args = {
        .callback = &Esp32HardwarePwm::timerIsr,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "SpreadSpectrum"
    };
    esp_timer_handle_t timer_handle;
    esp_err_t result = esp_timer_create(&timer_args, &timer_handle);
    if (result != ESP_OK) {
        debug_e("Failed to create timer: %s", esp_err_to_name(result));
        return false;
    }
    esp_timer_start_periodic(timer_handle, interval_us);

    if (!timer_handle) {
        debug_e("Failed to create timer");
        return false;
    }

    
    return true;
}

void IRAM_ATTR Esp32HardwarePwm::timerIsr(void* arg) {
    auto* self = static_cast<Esp32HardwarePwm*>(arg);
    self->handleSpreadSpectrum();
}

void IRAM_ATTR Esp32HardwarePwm::handleSpreadSpectrum() {
    int width = (spreadSpectrum_.WidthPercent * timer_.frequency) / 100;
    int r = esp_random() % (2 * width + 1) - width; // r in [-width, +width]
    ledc_set_freq(timer_.speed_mode, timer_.timer_num, timer_.frequency + r);
}

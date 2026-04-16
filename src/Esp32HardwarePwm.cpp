/**
 * @author  Peter Jakobs http://github.com/pljakobs
 */

#include "Esp32HardwarePwm.h"
#include <debug_progmem.h>
#include <driver/periph_ctrl.h>
#include <esp_err.h>
#include <esp_random.h>
#include <esp_timer.h>
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
 * - Hardware fading leveraging the ledc_set_fade_and_start 
 *
 * toDo:
 * - currently, fade does not provide any callbacks, might be worthwhile to implement them in the future
 *   just callign that from the internal fadeDoneCallback() might be risky as that's run in an interrupt
 * -  
 *
 ****/

namespace
{
/**
 * @brief Calculate maximum duty value for given resolution
 */
uint32_t maxDutyForResolution(ledc_timer_bit_t resolution)
{
	return (1U << resolution) - 1;
}

/**
 * @brief Convert frequency to period in microseconds
 */
uint32_t frequencyToPeriod(uint32_t frequency)
{
	return (frequency == 0) ? 0 : (1000000 / frequency);
}

/**
 * @brief Convert period in microseconds to frequency
 */
uint32_t periodToFrequency(uint32_t period_us)
{
	return (period_us == 0) ? 0 : (1000000 / period_us);
}

} // anonymous namespace

//=============================================================================
// Esp32HardwarePwm Implementation
//=============================================================================

// ---------------------------------------------------------------------------
// Constructors / destructor
// ---------------------------------------------------------------------------

Esp32HardwarePwm::Esp32HardwarePwm(std::vector<uint8_t>& pins) : Esp32HardwarePwm(pins, Config{})
{
}

Esp32HardwarePwm::Esp32HardwarePwm(std::vector<uint8_t>& pins, const Config& config)
{
	timer_ = config.timer;
	spreadSpectrum_ = config.spreadSpectrum;
	phaseShift_ = config.phaseShift;
	pins_.resize(pins.size());

	// basic sanity checks
	if(pins.size() == 0) {
		debug_e("Pin count must be positive");
		return;
	}
	if(pins.size() >= SOC_LEDC_CHANNEL_NUM) {
		debug_e("Pin count exceeds available LEDC channels");
		return;
	}
	if(pins.size() + config.channelStart > SOC_LEDC_CHANNEL_NUM) {
		debug_e("Channel range exceeds available channels");
		return;
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

	for(uint8_t i = 0; i < pins.size(); ++i) {
		auto& cfg = pins_[i];
		cfg.gpioPin = pins[i];
		cfg.channel = (ledc_channel_t)(config.channelStart + i);

		switch(phaseShift_.mode) {
		case PhaseShiftMode::AUTO:
			cfg.hpoint = calculateHpoint(i);
			break;
		case PhaseShiftMode::MANUAL:
			cfg.hpoint = (i < config.phaseShift.manual_hpoints.size()) ? config.phaseShift.manual_hpoints[i] : 0;
			break;
		case PhaseShiftMode::OFF:
		default:
			cfg.hpoint = 0;
			break;
		}
	}

	debug_i("PWM Constructor Configuration:");
	debug_i("  Timer: num=%d, resolution=%d, freq=%d, speed_mode=%d, clk_cfg=%d", timer_.timer_num, timer_.resolution,
			timer_.frequency, timer_.speed_mode, timer_.clk_cfg);
	debug_i("  SpreadSpectrum: mode=%d, WidthPercent=%d, Subsampling=%d", spreadSpectrum_.mode,
			spreadSpectrum_.WidthPercent, spreadSpectrum_.Subsampling);
	debug_i("  PhaseShiftMode: %d", static_cast<int>(phaseShift_.mode));
	debug_i("  Channel start: %d", config.channelStart);
	debug_i("  Pins : %i", pins.size());
	for(size_t i = 0; i < pins.size(); ++i) {
		debug_i("    Pin[%i]: %d, hpoint: %d", i, pins[i], pins_[i].hpoint);
	}

	initialized_ = initialize();
}

Esp32HardwarePwm::~Esp32HardwarePwm()
{
	if(fadeInstalled_) {
		ledc_fade_func_uninstall();
	}

	if(initialized_) {
		// Stop all channels
		for(const auto& pin : pins_) {
			if(pin.isActive) {
				ledc_stop(timer_.speed_mode, pin.channel, 0);
			}
		}

		ledc_timer_config_t timer_config = {
			.speed_mode = timer_.speed_mode, .timer_num = timer_.timer_num, .deconfigure = true};
		auto result = ledc_timer_config(&timer_config);
	}
}

// ---------------------------------------------------------------------------
// Timer / global configuration
// ---------------------------------------------------------------------------

bool Esp32HardwarePwm::setFrequency(uint32_t frequency)
{
	if(!initialized_ || pins_.empty()) {
		return false;
	}

	// Update frequency for the timer used by our channels

	esp_err_t result = ledc_set_freq(timer_.speed_mode, timer_.timer_num, frequency);

	if(result == ESP_OK) {
		timer_.frequency = frequency;
		// debug_i("Set frequency to %d Hz", frequency);
		return true;
	} else {
		debug_e("Failed to set frequency: %s", esp_err_to_name(result));
		return false;
	}
}

uint32_t Esp32HardwarePwm::getFrequency() const
{
	if(!initialized_) {
		return 0;
	}

	return ledc_get_freq(timer_.speed_mode, timer_.timer_num);
}

bool Esp32HardwarePwm::setPeriod(uint32_t period_us)
{
	uint32_t frequency = periodToFrequency(period_us);
	return setFrequency(frequency);
}

uint32_t Esp32HardwarePwm::getPeriod() const
{
	return frequencyToPeriod(getFrequency());
}

uint32_t Esp32HardwarePwm::getMaxDuty() const
{
	return maxDutyForResolution(timer_.resolution);
}

uint8_t Esp32HardwarePwm::getResolution() const
{
	return static_cast<uint8_t>(timer_.resolution);
}

void Esp32HardwarePwm::update()
{
	// todo: this does not do anything meaningful
	if(!initialized_) {
		return;
	}

	// Update all channels
	for(const auto& pin : pins_) {
		if(pin.isActive) {
			ledc_update_duty(timer_.speed_mode, pin.channel);
		}
	}
}

void Esp32HardwarePwm::stopAll(bool idle_level)
{
	for(auto& pin : pins_) {
		ledc_stop(timer_.speed_mode, pin.channel, idle_level);
		pin.isActive = false;
	}
}

// ---------------------------------------------------------------------------
// Primary interface — channel-indexed
// ---------------------------------------------------------------------------

bool Esp32HardwarePwm::setDutyChan(uint8_t channel, uint32_t duty, bool update_immediately)
{
	if(!initialized_ || channel >= pins_.size()) {
		return false;
	}

	auto& cfg = pins_[channel];
	if(cfg.currentDuty == duty) {
		return true; // no change
	}

	uint32_t max_duty = getMaxDuty();
	if(duty > max_duty) {
		debug_w("Duty %d exceeds maximum %d, clamping", duty, max_duty);
		duty = max_duty;
	}

	debug_i("Setting duty for pin %d channel %d: %d", cfg.gpioPin, cfg.channel, duty);
	cfg.currentDuty = duty;
	ledc_set_duty(timer_.speed_mode, cfg.channel, duty);

	if(update_immediately) {
		ledc_update_duty(timer_.speed_mode, cfg.channel);
	}
	return true;
}

uint32_t Esp32HardwarePwm::getDutyChan(uint8_t channel)
{
	if(!initialized_) {
		return 0;
	}
	//debug_i("Getting duty for channel %d", channel);
	if(channel >= pins_.size()) {
		return 0;
	}
	return ledc_get_duty(timer_.speed_mode, pins_[channel].channel);
}

bool Esp32HardwarePwm::setPhaseShiftChan(uint8_t channel, uint32_t phase_shift, bool update_immediately)
{
	if(!initialized_ || channel >= pins_.size()) {
		return false;
	}

	auto& cfg = pins_[channel];
	cfg.hpoint = phase_shift;

	ledc_set_duty_with_hpoint(timer_.speed_mode, cfg.channel, cfg.currentDuty, cfg.hpoint);
	if(update_immediately) {
		ledc_update_duty(timer_.speed_mode, cfg.channel);
	}
	return true;
}

bool Esp32HardwarePwm::enableFade()
{
	if(fadeInstalled_)
		return true;

	esp_err_t result = ledc_fade_func_install(0);
	if(result == ESP_OK || result == ESP_ERR_INVALID_STATE) {
		fadeInstalled_ = true;
		return true;
	}
	debug_e("Failed to enable fade: %s", esp_err_to_name(result));
	return false;
}

void Esp32HardwarePwm::disableFade()
{
	if(fadeInstalled_) {
		ledc_fade_func_uninstall(); // disable hardware fade
		fadeInstalled_ = false;
		debug_i("Fade functionality disabled");
	}
}

bool Esp32HardwarePwm::fadeToValueChan(uint8_t channel_idx, uint32_t target_duty, uint32_t fade_time_ms)
{
	if(!initialized_ || !fadeInstalled_ || channel_idx >= pins_.size())
		return false;

	uint32_t max_duty = getMaxDuty();
	if(target_duty > max_duty)
		target_duty = max_duty;

	fadeDone_[channel_idx] = false;
	esp_err_t result = ledc_set_fade_time_and_start(timer_.speed_mode, pins_[channel_idx].channel, target_duty,
													fade_time_ms, LEDC_FADE_NO_WAIT);
	if(result != ESP_OK) {
		fadeDone_[channel_idx] = true;
		debug_e("fadeToValueChan failed: %s", esp_err_to_name(result));
		return false;
	}
	return true;
}

bool Esp32HardwarePwm::fadeToPercentChan(uint8_t channel_idx, float target_pct, uint32_t fade_time_ms)
{
	if(target_pct < 0.0f)
		target_pct = 0.0f;
	if(target_pct > 100.0f)
		target_pct = 100.0f;
	uint32_t target_duty = static_cast<uint32_t>((target_pct / 100.0f) * getMaxDuty());
	return fadeToValueChan(channel_idx, target_duty, fade_time_ms);
}

bool Esp32HardwarePwm::isFadingChan(uint8_t channel_idx) const
{
	if(channel_idx >= pins_.size())
		return false;
	return !fadeDone_[channel_idx];
}

// ---------------------------------------------------------------------------
// Legacy interface — GPIO-pin-indexed
// ---------------------------------------------------------------------------

bool Esp32HardwarePwm::start(uint8_t pin)
{
	if(!initialized_) {
		return false;
	}
	return true;
	// todo: do something useful
}

bool Esp32HardwarePwm::stop(uint8_t pin, bool idle_level)
{
	if(!initialized_) {
		return false;
	}

	auto pin_config = getPinConfig(pin);
	if(!pin_config) {
		debug_e("Pin %d not found", pin);
		return false;
	}
	esp_err_t result = ledc_stop(timer_.speed_mode, pin_config->channel, idle_level);

	if(result == ESP_OK) {
		pin_config->isActive = false;
		return true;
	}

	return false;
}

void Esp32HardwarePwm::startAll()
{
	// todo: do something useful
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

bool Esp32HardwarePwm::initialize()
{
	debug_i("Esp32HardwarePwm::initialize");

	// Enable LEDC peripheral
	periph_module_enable(PERIPH_LEDC_MODULE);

	if(!enableFade())
		return false;

	debug_i("initialize timer");
	// initialize the timer
	ledc_timer_config_t timer_config = {.speed_mode = timer_.speed_mode,
										.duty_resolution = timer_.resolution,
										.timer_num = timer_.timer_num,
										.freq_hz = timer_.frequency,
										.clk_cfg = timer_.clk_cfg};

	auto result = ledc_timer_config(&timer_config);
	if(result != ESP_OK) {
		debug_e("Failed to configure timer: %s", esp_err_to_name(result));
		return false;
	}

	// Allocate channels from resource manager
	debug_i("Esp32HardwarePwm::initialize - getting Channels");

	if(spreadSpectrum_.mode != SpreadSpectrumMode::OFF) {
		// Configure spread spectrum
		debug_i("Configuring spread spectrum");
		setupSpreadSpectrum(timer_.frequency, spreadSpectrum_);
	}

	// Configure each channel
	for(size_t i = 0; i < pins_.size(); ++i) {
		auto& cfg = pins_[i];
		ledc_channel_config_t channel_config = {.gpio_num = cfg.gpioPin,
												.speed_mode = timer_.speed_mode,
												.channel = cfg.channel,
												.intr_type = LEDC_INTR_DISABLE,
												.timer_sel = timer_.timer_num,
												.duty = 0,
												.hpoint = cfg.hpoint};
		debug_i("Channel config: gpio_num=%d, speed_mode=%d, channel=%d, timer_sel=%d, duty=%d, hpoint=%d",
				channel_config.gpio_num, channel_config.speed_mode, channel_config.channel, channel_config.timer_sel,
				channel_config.duty, channel_config.hpoint);

		auto result = ledc_channel_config(&channel_config);
		if(result != ESP_OK) {
			debug_e("Failed to configure pin %d: %s", channel_config.gpio_num, esp_err_to_name(result));
			return false;
		}

		cfg.isActive = true;

		debug_i("configured pin %d:\n  channel: %d\n  hpoint: %d", cfg.gpioPin, cfg.channel, cfg.hpoint);

		ledc_cbs_t cbs = {.fade_cb = &Esp32HardwarePwm::fadeDoneCallback};
		ledc_cb_register(timer_.speed_mode, cfg.channel, &cbs, this);
		fadeDone_[i] = true;
	}

	debug_i("Initialized PWM with %d pins", pins_.size());
	return true;
}

bool Esp32HardwarePwm::setupSpreadSpectrum(int frequency, SpreadSpectrumConfig& config)
{
	spreadSpectrum_ = config;
	int interval_us = 1000000 * spreadSpectrum_.Subsampling / frequency;
	esp_timer_create_args_t timer_args = {.callback = &Esp32HardwarePwm::timerIsr,
										  .arg = this,
										  .dispatch_method = ESP_TIMER_TASK,
										  .name = "SpreadSpectrum"};
	esp_timer_handle_t timer_handle = nullptr;
	esp_err_t result = esp_timer_create(&timer_args, &timer_handle);
	if(result != ESP_OK) {
		debug_e("Failed to create timer: %s", esp_err_to_name(result));
		return false;
	}
	esp_timer_start_periodic(timer_handle, interval_us);

	if(!timer_handle) {
		debug_e("Failed to create timer");
		return false;
	}

	return true;
}

bool Esp32HardwarePwm::fadeDoneCallback(const ledc_cb_param_t* param, void* arg)
{
	auto* self = static_cast<Esp32HardwarePwm*>(arg);
	// param->channel is the hardware ledc_channel_t — find the pins_ index
	for(size_t i = 0; i < self->pins_.size(); ++i) {
		if(self->pins_[i].channel == param->channel) {
			self->fadeDone_[i] = true;
			break;
		}
	}
	return false; // no higher-priority task woken
}

void Esp32HardwarePwm::timerIsr(void* arg)
{
	auto* self = static_cast<Esp32HardwarePwm*>(arg);
	self->handleSpreadSpectrum();
}

void Esp32HardwarePwm::handleSpreadSpectrum()
{
	int width = (spreadSpectrum_.WidthPercent * timer_.frequency) / 100;
	int r = esp_random() % (2 * width + 1) - width; // r in [-width, +width]
	ledc_set_freq(timer_.speed_mode, timer_.timer_num, timer_.frequency + r);
}

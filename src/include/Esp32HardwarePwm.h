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
#include <Delegate.h>

// ---------------------------------------------------------------------------
// Per-channel fade queue depth — override before including this header.
// ---------------------------------------------------------------------------
#ifndef FADE_QUEUE_DEPTH
#define FADE_QUEUE_DEPTH 10
#endif

// ---------------------------------------------------------------------------
// Debug level control for Esp32HardwarePwm
// Define HW_PWM_DEBUG before including this header or via compiler flag.
// Uses Sming's ENABLE_DEBUG mechanism (debug_progmem.h) in the .cpp file.
//   0 = no logging
//   1 = errors only   (debug_e)
//   2 = errors + info (debug_e, debug_i)
//   3 = full          (debug_e, debug_i, debug_d)
// ---------------------------------------------------------------------------
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 1
#endif

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
class Esp32HardwarePwm
{
public:
	static constexpr uint8_t BadChannel = 0xff; ///< Invalid PWM channel indicator

	/**
     * @brief Defines PWM duty cycle percentage (0.0 = off, 100.0 = full on)
     */
	using DutyCycle = float;

	// -----------------------------------------------------------------------
	// Fade queue types
	// -----------------------------------------------------------------------

	enum class QueueMode : uint8_t {
		FIFO,   ///< Queue drains and stops; onQueueEmpty fires when exhausted
		CYCLIC, ///< Playback loops back to entry 0 endlessly; onCyclicWrap fires on each loop
	};

	// -----------------------------------------------------------------------
	// Configuration types — declared before constructors so they are visible
	// in constructor parameter lists.
	// -----------------------------------------------------------------------

	enum class PhaseShiftMode : uint8_t {
		OFF,	///< No phase shifting
		AUTO,   ///< Automatic phase shifting based on channel index
		MANUAL, ///< Manual phase shifting using provided hpoint values
	};

	enum class SpreadSpectrumMode : uint8_t {
		OFF, ///< Spread spectrum disabled
		ON,  ///< Spread spectrum enabled
	};

	struct PhaseShiftConfig {
		PhaseShiftMode mode = PhaseShiftMode::OFF; ///< Phase shift mode
		std::vector<int> manual_hpoints = {};	  ///< hpoint values for MANUAL mode, one per pin
	};

	struct SpreadSpectrumConfig {
		SpreadSpectrumMode mode = SpreadSpectrumMode::OFF; ///< Spread spectrum mode
		uint8_t WidthPercent = 0;						   ///< Frequency deviation as percentage of base frequency
		uint16_t Subsampling = 0;						   ///< Number of PWM cycles between frequency updates
	};

	struct TimerConfig {
		ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;	///< LEDC speed mode
		ledc_timer_bit_t resolution = LEDC_TIMER_10_BIT; ///< Duty resolution in bits
		ledc_timer_t timer_num = LEDC_TIMER_0;			 ///< LEDC timer index
		uint32_t frequency = 1000;						 ///< PWM frequency in Hz
		ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;			 ///< Clock source
	};

	struct Config {
		ledc_channel_t channelStart = LEDC_CHANNEL_0; ///< First LEDC channel to allocate
		TimerConfig timer = {};						  ///< Timer configuration
		PhaseShiftConfig phaseShift = {};			  ///< Phase shift configuration
		SpreadSpectrumConfig spreadSpectrum = {};	 ///< Spread spectrum configuration
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

	// -----------------------------------------------------------------------
	// Timer / global configuration
	// -----------------------------------------------------------------------

	/** @brief Change PWM frequency for all channels
     * @param frequency New frequency in Hz
     * @return true if successful, false otherwise
     * @note This affects all channels sharing the same timer
     */
	bool setFrequency(uint32_t frequency);

	/** @brief Get current PWM frequency
     * @return Frequency in Hz
     */
	uint32_t getFrequency() const;

	/** @brief Set PWM period in microseconds
     * @param period_us Period in microseconds
     * @return true if successful, false otherwise
     */
	bool setPeriod(uint32_t period_us);

	/** @brief Get PWM period in microseconds
     * @return Period in microseconds
     */
	uint32_t getPeriod() const;

	/** @brief Get maximum duty cycle value
     * @return Maximum duty value based on resolution
     */
	uint32_t getMaxDuty() const;

	/** @brief Get duty resolution in bits
     * @return Resolution in bits (1-20)
     */
	uint8_t getResolution() const;

	/** @brief Get total number of configured channels
     * @return Number of channels
     */
	uint8_t getPinCount() const
	{
		return static_cast<uint8_t>(pins_.size());
	}

	/** @brief Check if PWM instance is properly initialized
     * @return true if initialized, false otherwise
     */
	bool isInitialized() const
	{
		return initialized_;
	}

	/** @brief Apply pending duty changes to all channels
     * @note Only needed when update_immediately was set to false
     */
	void update();

	/** @brief Stop PWM output on all channels
     * @param idle_level Level to set pins when stopped (0 or 1)
     */
	void stopAll(bool idle_level = false);

	// -----------------------------------------------------------------------
	// Primary interface — channel-indexed (preferred)
	// Channel index is the 0-based position in the pins[] vector passed to
	// the constructor, independent of GPIO number or LEDC hardware channel.
	// -----------------------------------------------------------------------

	/** @brief Set duty cycle for a channel (absolute value)
     * @param channel Channel index
     * @param duty Duty cycle value (0 to getMaxDuty())
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
	bool setDutyChan(uint8_t channel, uint32_t duty, bool update_immediately = true);

	/** @brief Get duty cycle for a channel (absolute value)
     * @param channel Channel index
     * @return Current duty cycle value (0 to getMaxDuty())
     */
	uint32_t getDutyChan(uint8_t channel);

	/** @brief Set duty cycle for a channel as a percentage
     * @param channel Channel index
     * @param percentage Duty cycle percentage (0.0 to 100.0)
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
	bool setDutyChanPercent(uint8_t channel, DutyCycle percentage, bool update_immediately = true)
	{
		if(percentage < 0.0f)
			percentage = 0.0f;
		if(percentage > 100.0f)
			percentage = 100.0f;
		uint32_t duty = static_cast<uint32_t>(percentage * getMaxDuty() / 100.0f);
		return setDutyChan(channel, duty, update_immediately);
	}

	/** @brief Get duty cycle for a channel as a percentage
     * @param channel Channel index
     * @return Duty cycle percentage (0.0 to 100.0)
     */
	DutyCycle getDutyChanPercent(uint8_t channel)
	{
		uint32_t duty = getDutyChan(channel);
		uint32_t max_duty = getMaxDuty();
		if(max_duty == 0)
			return 0.0f;
		return (static_cast<float>(duty) / max_duty) * 100.0f;
	}

	/** @brief Set phase shift for a channel (absolute hpoint value)
     * @param channel Channel index
     * @param phase_shift Phase shift value in PWM resolution counts (0 to getMaxDuty())
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
	bool setPhaseShiftChan(uint8_t channel, uint32_t phase_shift, bool update_immediately = true);

	/** @brief Set phase shift for a channel as a percentage of the PWM period
     * @param channel Channel index
     * @param percentage Phase shift percentage (0.0 = no shift, 100.0 = full period)
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
	bool setPhaseShiftChanPercent(uint8_t channel, DutyCycle percentage, bool update_immediately = true)
	{
		if(percentage < 0.0f)
			percentage = 0.0f;
		if(percentage > 100.0f)
			percentage = 100.0f;
		uint32_t phase_shift = static_cast<uint32_t>(percentage * getMaxDuty() / 100.0f);
		return setPhaseShiftChan(channel, phase_shift, update_immediately);
	}

	/** @brief Enable hardware fade functionality
     * @return true if successful, false otherwise
     * @note Called automatically by the constructor; exposed for manual control.
     */
	bool enableFade();

	/** @brief Disable hardware fade functionality */
	void disableFade();

	/** @brief Start hardware linear fade on a channel (absolute target)
     * @param channel_idx Channel index
     * @param target_duty Target duty value (0 to getMaxDuty())
     * @param fade_time_ms Duration in milliseconds
     * @return true if started successfully
     */
	bool fadeToValueChan(uint8_t channel_idx, uint32_t target_duty, uint32_t fade_time_ms);

	/** @brief Start hardware linear fade on a channel (percentage target)
     * @param channel_idx Channel index
     * @param target_pct Target duty as percentage (0.0–100.0)
     * @param fade_time_ms Duration in milliseconds
     * @return true if started successfully
     */
	bool fadeToPercentChan(uint8_t channel_idx, DutyCycle target_pct, uint32_t fade_time_ms);

	/** @brief Returns true while a hardware fade is in progress on the given channel */
	bool isFadingChan(uint8_t channel_idx) const;

	// -----------------------------------------------------------------------
	// Fade queue interface
	// -----------------------------------------------------------------------

	/** @brief Set queue mode for a channel.
	 * Must be called before filling the queue. Changing mode while the queue
	 * is running has undefined behaviour.
	 */
	void setQueueMode(uint8_t channel, QueueMode mode);

	/** @brief Get current queue mode for a channel */
	QueueMode getQueueMode(uint8_t channel) const;

	/** @brief Enqueue a fade on a channel (absolute duty target).
	 * For FIFO queues (autoStart=true, the default), playback starts automatically
	 * on the first entry when the channel is idle.  For CYCLIC queues
	 * (autoStart=false by default), call startQueue() after seeding all entries.
	 * Returns false if the queue is full.
	 */
	bool queueFadeChan(uint8_t channel, uint32_t targetDuty, uint32_t fadeTimeMs);

	/** @brief Enqueue a fade on a channel (percentage target, 0.0–100.0) */
	bool queueFadePercentChan(uint8_t channel, float targetPct, uint32_t fadeTimeMs)
	{
		if(targetPct < 0.0f)
			targetPct = 0.0f;
		if(targetPct > 100.0f)
			targetPct = 100.0f;
		return queueFadeChan(channel, static_cast<uint32_t>(targetPct / 100.0f * getMaxDuty()), fadeTimeMs);
	}

	/** @brief Return number of entries currently in the queue for a channel */
	uint16_t getQueueEntries(uint8_t channel) const;

	/** @brief Clear the queue for a channel and reset mode to FIFO.
	 * The currently-running hardware fade (if any) completes normally, but no
	 * further queue entries are started and no callbacks fire afterwards.
	 * Queue capacity is preserved.
	 */
	void resetQueue(uint8_t channel);

	/** @brief Explicitly start a queued sequence.
	 * Must be called after seeding a CYCLIC queue (autoStart=false).  For FIFO
	 * queues with autoStart=true this is normally not needed, but can be used
	 * to restart an idle queue.  Returns false if the queue is empty or the
	 * channel is already fading.
	 */
	bool startQueue(uint8_t channel);

	/** @brief Set the maximum number of entries for a channel's queue.
	 * Can only be changed while the queue is empty.  Returns false if the
	 * queue is not empty or depth is zero.
	 */
	bool setQueueCapacity(uint8_t channel, uint16_t depth);

	/** @brief Get the maximum number of entries for a channel's queue */
	uint16_t getQueueCapacity(uint8_t channel) const;

	/** @brief Override the auto-start behaviour for a channel's queue.
	 * setQueueMode() sets autoStart automatically (true for FIFO, false
	 * for CYCLIC).  Use this to override that default.
	 */
	void setQueueAutoStart(uint8_t channel, bool autoStart);

	/** @brief Returns true if the queue starts automatically on first queueFadeChan() call */
	bool getQueueAutoStart(uint8_t channel) const;

	/** @brief Callback fired after every individual fade completes (even if more are queued) */
	void setOnFadeDoneCallback(Delegate<void(uint8_t)> cb)
	{
		onFadeDone_ = cb;
	}

	/** @brief Callback fired when a FIFO queue drains to empty */
	void setOnQueueEmptyCallback(Delegate<void(uint8_t)> cb)
	{
		onQueueEmpty_ = cb;
	}

	/** @brief Callback fired each time a CYCLIC queue wraps back to entry 0 */
	void setOnCyclicWrapCallback(Delegate<void(uint8_t)> cb)
	{
		onCyclicWrap_ = cb;
	}

	
	// -----------------------------------------------------------------------
	// Legacy interface — GPIO-pin-indexed (use channel interface for new code)
	// -----------------------------------------------------------------------

	/** @brief Set PWM duty cycle for a specific pin (legacy)
     * @param pin GPIO pin number
     * @param duty Duty cycle value (0 to getMaxDuty())
     * @param update_immediately Apply changes immediately (default: true)
     * @return true if successful, false otherwise
     */
	bool setDuty(uint8_t pin, uint32_t duty, bool update_immediately = true)
	{
		int idx = getPinIndex(pin);
		if(idx < 0)
			return false;
		return setDutyChan((uint8_t)idx, duty, update_immediately);
	}

	/** @brief Get PWM duty cycle for a specific pin (legacy)
     * @param pin GPIO pin number
     * @return Current duty cycle value
     */
	uint32_t getDuty(uint8_t pin)
	{
		int idx = getPinIndex(pin);
		if(idx < 0)
			return 0;
		return getDutyChan((uint8_t)idx);
	}

	/** @brief Arduino-style analogWrite (legacy)
     * @param pin GPIO pin number
     * @param duty Duty cycle value
     * @return true if successful, false otherwise
     */
	bool analogWrite(uint8_t pin, uint32_t duty)
	{
		return setDuty(pin, duty);
	}

	/** @brief Start PWM output on a specific pin (legacy)
     * @param pin GPIO pin number
     * @return true if successful, false otherwise
     */
	bool start(uint8_t pin);

	/** @brief Stop PWM output on a specific pin (legacy)
     * @param pin GPIO pin number
     * @param idle_level Level to set pin when stopped (0 or 1)
     * @return true if successful, false otherwise
     */
	bool stop(uint8_t pin, bool idle_level = false);

	/** @brief Start PWM output on all pins (legacy) */
	void startAll();

private:
	struct FadeEntry {
		uint32_t targetDuty;
		uint32_t fadeTimeMs;
	};

	struct ChannelFadeQueue {
		std::vector<FadeEntry> entries;  ///< Ring-buffer storage (size = queue capacity)
		uint16_t head = 0;		 ///< Next entry to consume
		uint16_t tail = 0;		 ///< Next free write slot
		uint16_t count = 0;		 ///< FIFO: decrements on pop; CYCLIC: fixed after seeding
		uint16_t cycleLen = 0;	 ///< CYCLIC: number of entries in the cycle
		QueueMode mode = QueueMode::FIFO;
		bool autoStart = true;	 ///< If true, playback starts on first queueFadeChan(); false requires startQueue()
	};

	struct PinConfig {
		uint8_t gpioPin = 0;					 ///< GPIO pin number
		ledc_channel_t channel = LEDC_CHANNEL_0; ///< LEDC hardware channel
		uint32_t currentDuty = 0;				 ///< Last duty value written
		uint32_t targetDuty = 0;				 ///< Target duty for in-progress fade
		int hpoint = 0;							 ///< Phase shift hpoint
		bool isActive = false;					 ///< True when channel is running
	};

	TimerConfig timer_;
	SpreadSpectrumConfig spreadSpectrum_;
	PhaseShiftConfig phaseShift_;
	std::vector<PinConfig> pins_;
	std::vector<ChannelFadeQueue> fadeQueues_;

	bool initialized_ = false;
	bool fadeInstalled_ = false;

	// Per-channel fade-done flags, set from LEDC fade callback (ISR-safe)
	std::array<volatile bool, SOC_LEDC_CHANNEL_NUM> fadeDone_{};

	// ISR → task handoff for fade completion
	volatile uint32_t pendingFadeCallbacks_ = 0;
	volatile bool fadeCallbackQueued_ = false;

	// Application-level callbacks
	Delegate<void(uint8_t)> onFadeDone_;
	Delegate<void(uint8_t)> onQueueEmpty_;
	Delegate<void(uint8_t)> onCyclicWrap_;

	/**
     * @brief Initialize PWM instance
     * @param pins Array of GPIO pins
     * @param pin_count Number of pins
     * @return true if successful, false otherwise
     */
	bool initialize();

	int getPinIndex(uint8_t gpioPin) const
	{
		for(size_t i = 0; i < pins_.size(); ++i) {
			if(pins_[i].gpioPin == gpioPin)
				return (int)i;
		}
		return -1;
	}

	/**
     * @brief Get channel info for a specific pin
     * @param pin GPIO pin number
     * @return Pointer to channel info, or nullptr if pin not found
     */
	PinConfig* getPinConfig(uint8_t gpioPin)
	{
		for(auto& pin : pins_) {
			if(pin.gpioPin == gpioPin)
				return &pin;
		}
		return nullptr;
	}

	/**
     * @brief Calculate hpoint for phase shifting
     * @param channel_index Index of the channel
     * @return Calculated hpoint value
     */
	int calculateHpoint(uint8_t channel_index) const
	{
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
	void handleSpreadSpectrum();

	// Fade callback registered with ledc_cb_register per channel — runs in ISR context
	static bool IRAM_ATTR fadeDoneCallback(const ledc_cb_param_t* param, void* arg);

	// Task-context dispatcher — deferred from ISR via System.queueCallback
	static void dispatchFadeCallbacks(uint32_t param);

	// Dequeue the head entry from a FIFO queue (advances head, decrements count)
	FadeEntry dequeueFifo(ChannelFadeQueue& q);

	// Start the next fade from the queue; returns false if queue empty
	bool startNextFade(uint8_t channel_idx);

	// esp_timer callback — runs in task context, static wrapper required for C function pointer
	static void spreadSpectrumTimerCb(void* arg);
};

/** @} */

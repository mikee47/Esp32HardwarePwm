/*
 * File: Esp32 HardwarePwm fade-queue example
 * Author: https://github.com/pljakobs
 *
 * Demonstrates the per-channel fade queue API:
 *
 *   Channel 0 — FIFO mode (default)
 *     Three fades are pre-loaded; the library chains them automatically.
 *     onQueueEmpty fires when the last entry completes.
 *
 *   Channel 1 — CYCLIC mode
 *     Three entries loop forever.  onCyclicWrap fires on each full cycle.
 *     After three complete cycles resetQueue() stops the channel.
 *
 *   onFadeDone fires after every individual fade on any channel.
 */
#include <SmingCore.h>
#include <Esp32HardwarePwm.h>

namespace
{
// Adjust to output-capable pins on your board.
// Avoid GPIO 6-11 (SPI flash) and GPIO 34-39 (input-only on ESP32 classic).
std::vector<uint8_t> pinList{13, 12, 14, 27, 26};

// clang-format off
Esp32HardwarePwm pwm(pinList, Esp32HardwarePwm::Config{
	.timer = {
		.resolution = LEDC_TIMER_10_BIT,
		.frequency  = 4000,
	},
	.phaseShift = {
		.mode = Esp32HardwarePwm::PhaseShiftMode::OFF,
	},
	.spreadSpectrum = {
		.mode = Esp32HardwarePwm::SpreadSpectrumMode::OFF,
	},
});
// clang-format on

constexpr uint32_t FADE_MS = 1600; ///< Duration of each individual fade step

// Track how many complete CYCLIC loops have fired on channel 1
uint8_t cyclicWrapCount = 0;
constexpr uint8_t MAX_CYCLIC_LOOPS = 7; ///< Stop channel 1 after this many loops

void setupFadeQueueDemo()
{
	if(!pwm.isInitialized()) {
		Serial.println(_F("PWM not initialized — check pin list"));
		return;
	}

	// ------------------------------------------------------------------
	// Shared callback: fires after every individual fade on any channel
	// ------------------------------------------------------------------
	pwm.setOnFadeDoneCallback([](uint8_t ch) {
		Serial << _F("onFadeDone   ch=") << ch << _F("  duty=") << pwm.getDutyChan(ch) << _F("  queued=")
			   << pwm.getQueueEntries(ch) << endl;
	});

	// ------------------------------------------------------------------
	// FIFO callback: fires once when channel 0's queue drains to empty
	// ------------------------------------------------------------------
	pwm.setOnQueueEmptyCallback(
		[](uint8_t ch) { Serial << _F("onQueueEmpty ch=") << ch << _F(" — FIFO sequence complete") << endl; });

	// ------------------------------------------------------------------
	// CYCLIC callback: fires each time channel 1 loops back to entry 0
	// ------------------------------------------------------------------
	pwm.setOnCyclicWrapCallback([](uint8_t ch) {
		++cyclicWrapCount;
		Serial << _F("onCyclicWrap ch=") << ch << _F("  loop #") << cyclicWrapCount << endl;

		if(cyclicWrapCount >= MAX_CYCLIC_LOOPS) {
			Serial << _F("Reached ") << MAX_CYCLIC_LOOPS << _F(" loops — stopping channel ") << ch << endl;
			pwm.resetQueue(ch);
		}
	});

	// ------------------------------------------------------------------
	// Channel 0: FIFO — 3 fades pre-loaded, then channel goes idle
	// ------------------------------------------------------------------
	Serial.println(_F("Channel 0: FIFO — queuing 3 fades (100%->0%->50%->0%->100%->50%)"));
	// Mode defaults to FIFO; no setQueueMode call needed
	pwm.setQueueCapacity(0, 25);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS / 2);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS / 2);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS / 2);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS / 2);
	pwm.queueFadePercentChan(0, 0.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 100.0f, FADE_MS / 5);
	pwm.queueFadePercentChan(0, 50.0f, FADE_MS);
	pwm.queueFadePercentChan(0, 0.0f, 5000);
	// ------------------------------------------------------------------
	// Channel 1: CYCLIC — 3 entries loop until resetQueue() is called
	// ------------------------------------------------------------------
	Serial.println(_F("Channel 1: CYCLIC — 3-entry loop (0%→100%→0%→50%…)"));
	pwm.setQueueMode(1, Esp32HardwarePwm::QueueMode::CYCLIC);
	// Demonstrate runtime-configurable queue depth (4 slots instead of default 10)
	pwm.setQueueCapacity(1, 4);
	pwm.queueFadePercentChan(1, 100.0f, FADE_MS);
	pwm.queueFadePercentChan(1, 0.0f, FADE_MS);
	pwm.queueFadePercentChan(1, 50.0f, FADE_MS);
	pwm.queueFadePercentChan(1, 0.0f, FADE_MS);
	// All entries seeded — now start the cycle explicitly
	pwm.startQueue(1);
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

	Serial << _F("FadeQueue_HwPWM — freq=") << pwm.getFrequency() << _F(" Hz, resolution=") << pwm.getResolution()
		   << _F(" bits, maxDuty=") << pwm.getMaxDuty() << endl;

	setupFadeQueueDemo();
}

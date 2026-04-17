/*
 * File: Esp32 HardwarePwm advanced example
 * Author: https://github.com/pljakobs
 *
 * Demonstrates Esp32HardwarePwm with an explicit configuration:
 *   - 12-bit duty resolution  (max duty = 4095)
 *   - 4 kHz PWM frequency
 *   - Automatic phase shifting  (hpoints spread evenly across channels)
 *   - Spread spectrum disabled
 *
 * Uses the channel-indexed interface throughout.
 */
#include <SmingCore.h>
#include <Esp32HardwarePwm.h>

namespace
{
#define LED_PIN 3
// Channel index = 0-based position of LED_PIN in pinList.
// This is independent of the hardware LEDC channel number and of Config::channelStart.
// If you move LED_PIN to a different position in pinList, update LED_CHANNEL accordingly.
#define LED_CHANNEL 0

std::vector<uint8_t> pinList{13, 12, 14, 27, 26};

// Default duty percentages, one per channel
const Esp32HardwarePwm::DutyCycle defaultDutyPercent[]{50.0f, 95.0f, 50.0f, 85.0f, 10.0f};

// clang-format off
Esp32HardwarePwm pwm(pinList, Esp32HardwarePwm::Config{
	.timer = {
		.resolution = LEDC_TIMER_9_BIT,
		.frequency  = 44100,
	},
	.phaseShift = {
		.mode = Esp32HardwarePwm::PhaseShiftMode::AUTO,
	},
	.spreadSpectrum = {
		.mode = Esp32HardwarePwm::SpreadSpectrumMode::OFF,
	},
});
// clang-format on

// ---------------------------------------------------------------------------
// Fade queue demo
//   Channel 0: FIFO — 3 fades pre-loaded; callbacks log each event.
//   Channel 1: CYCLIC — 3 entries that loop indefinitely; wrap callback counts loops.
// ---------------------------------------------------------------------------
constexpr uint32_t FADE_TIME_MS = 800;

void setupFadeQueueDemo()
{
	// --- Shared callbacks (all channels) ---
	pwm.setOnFadeDoneCallback([](uint8_t ch) {
		Serial << _F("onFadeDone   ch=") << ch
		       << _F("  duty=") << pwm.getDutyChan(ch) << endl;
	});
	pwm.setOnQueueEmptyCallback([](uint8_t ch) {
		Serial << _F("onQueueEmpty ch=") << ch << _F(" — queue drained") << endl;
	});
	pwm.setOnCyclicWrapCallback([](uint8_t ch) {
		static uint32_t wrapCount = 0;
		Serial << _F("onCyclicWrap ch=") << ch
		       << _F("  wrap#") << ++wrapCount << endl;
	});

	// --- Channel 0: FIFO, 3 fades ---
	// Mode defaults to FIFO; no setFadeQueueMode call needed
	Serial << _F("Channel 0: queuing 3 FIFO fades") << endl;
	pwm.queueFadePercentChan(0, 100.0f, FADE_TIME_MS);
	pwm.queueFadePercentChan(0,   0.0f, FADE_TIME_MS);
	pwm.queueFadePercentChan(0,  50.0f, FADE_TIME_MS);

	// --- Channel 1: CYCLIC, 3 entries that loop ---
	Serial << _F("Channel 1: queuing 3 CYCLIC fades (loops forever)") << endl;
	pwm.setFadeQueueMode(1, Esp32HardwarePwm::FadeQueueMode::CYCLIC);
	pwm.queueFadePercentChan(1, 100.0f, FADE_TIME_MS);
	pwm.queueFadePercentChan(1,   0.0f, FADE_TIME_MS);
	pwm.queueFadePercentChan(1,  50.0f, FADE_TIME_MS);
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

	Serial << _F("PWM period = ") << pwm.getPeriod() << _F("us, freq = ") << pwm.getFrequency()
		   << _F(", resolution = ") << pwm.getResolution() << _F(" bits, max duty = ") << pwm.getMaxDuty() << endl;

	// Set default duty on every channel
	for(uint8_t ch = 0; ch < pwm.getPinCount(); ++ch) {
		pwm.setDutyChanPercent(ch, defaultDutyPercent[ch]);
	}

	Serial << _F("PWM output set on all ") << pwm.getPinCount() << _F(" channels.") << endl;

	setupFadeQueueDemo();
}

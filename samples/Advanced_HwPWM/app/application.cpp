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

std::vector<uint8_t> pinList{LED_PIN, 4, 5, 18, 19};

// Default duty percentages, one per channel
const Esp32HardwarePwm::DutyCycle defaultDutyPercent[]{50.0f, 95.0f, 50.0f, 85.0f, 10.0f};

// clang-format off
Esp32HardwarePwm pwm(pinList, Esp32HardwarePwm::Config{
	.timer = {
		.resolution = LEDC_TIMER_12_BIT,
		.frequency  = 4000,
	},
	.phaseShift = {
		.mode = Esp32HardwarePwm::PhaseShiftMode::AUTO,
	},
	.spreadSpectrum = {
		.mode = Esp32HardwarePwm::SpreadSpectrumMode::OFF,
	},
});
// clang-format on

constexpr uint32_t FADE_TIME_MS = 2000; // duration of each sweep leg

SimpleTimer procTimer;

void startNextFade()
{
	// Alternate between fading to 100% and back to 0%
	static bool countUp = true;
	pwm.fadeToPercentChan(LED_CHANNEL, countUp ? 100.0f : 0.0f, FADE_TIME_MS);
	countUp = !countUp;
}

void checkFadeDone()
{
	// When the hardware fade completes, immediately kick off the next one
	if(!pwm.isFadingChan(LED_CHANNEL)) {
		startNextFade();
	}
}

// ---------------------------------------------------------------------------
// Test routine 2: chase all channels
//   Every 200 ms: set current channel to 100% and start a 1 s fade to 0%,
//   then move to the next channel. Fades overlap — each channel is still
//   fading when the next one fires.
// ---------------------------------------------------------------------------
constexpr uint32_t CHASE_FADE_MS = 1000;
constexpr uint32_t CHASE_WAIT_MS = 200;

SimpleTimer chaseTimer;

void runChase()
{
	static uint8_t currentChannel = 0;

	pwm.setDutyChanPercent(currentChannel, 100.0f);
	pwm.fadeToPercentChan(currentChannel, 0.0f, CHASE_FADE_MS);

	currentChannel = (currentChannel + 1) % pwm.getPinCount();
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

	// --- choose one of the two test routines ---

	// Routine 1: single-channel hardware sweep on LED_CHANNEL
	Serial << _F("Routine 1: LED (pin ") << LED_PIN << _F(", channel ") << LED_CHANNEL
		   << _F(") sweeps 0–100% using hardware fade.") << endl;
	startNextFade();
	procTimer.initializeMs<20>(checkFadeDone).start();

	// Routine 2: chase — uncomment to use instead of routine 1
	// Serial << _F("Routine 2: chasing all ") << pwm.getPinCount() << _F(" channels.") << endl;
	// chaseTimer.initializeMs<CHASE_WAIT_MS>(runChase).start();
}

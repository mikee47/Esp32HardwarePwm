/*
 * File: Esp32 HardwarePwm example
 * Author: https://github.com/pljakobs
 *
 * Demonstrates Esp32HardwarePwm using the channel-indexed interface.
 * All duty values are expressed as percentages.
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

Esp32HardwarePwm pwm(pinList);

SimpleTimer procTimer;

void doPWM()
{
	static bool countUp = true;
	static Esp32HardwarePwm::DutyCycle pct = 0.0f;

	const Esp32HardwarePwm::DutyCycle step = 2.0f; // 2% per tick → 0–100% in 50 ticks

	if(countUp) {
		pct += step;
		if(pct >= 100.0f) {
			pct = 100.0f;
			countUp = false;
		}
	} else {
		pct -= step;
		if(pct <= 0.0f) {
			pct = 0.0f;
			countUp = true;
		}
	}

	pwm.setDutyChanPercent(LED_CHANNEL, pct);
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

	// Change PWM frequency / period if required:
	// pwm.setFrequency(500);
	// pwm.setPeriod(2000); // microseconds

	Serial << _F("PWM period = ") << pwm.getPeriod() << _F("us, freq = ") << pwm.getFrequency()
		   << _F(", resolution = ") << pwm.getResolution() << _F(" bits, max duty = ") << pwm.getMaxDuty() << endl;

	// Set default duty on every channel
	for(uint8_t ch = 0; ch < pwm.getPinCount(); ++ch) {
		pwm.setDutyChanPercent(ch, defaultDutyPercent[ch]);
	}

	Serial << _F("PWM output set on all ") << pwm.getPinCount() << _F(" channels.") << endl
		   << _F("LED (pin ") << LED_PIN << _F(", channel ") << LED_CHANNEL
		   << _F(") will sweep 0–100% in cycles.") << endl;

	procTimer.initializeMs<100>(doPWM).start();
}

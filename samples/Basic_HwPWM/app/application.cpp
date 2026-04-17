/*
 * File: Esp32 HardwarePwm example
 * Author: https://github.com/pljakobs
 *
 * Demonstrates Esp32HardwarePwm using the channel-indexed interface.
 * All duty values are expressed as percentages.
 */
#include <SmingCore.h>
#include <Esp32HardwarePwm.h>

#define LED_PIN 13
// Channel index = 0-based position of LED_PIN in pinList.
// This is independent of the hardware LEDC channel number and of Config::channelStart.
// If you move LED_PIN to a different position in pinList, update LED_CHANNEL accordingly.
#define LED_CHANNEL 0

// Note: GPIO 6-11 are reserved for SPI flash on ESP32 and must not be used.
std::vector<uint8_t> pinList{13, 12, 14, 27, 26};

// Default duty percentages, one per channel
const Esp32HardwarePwm::DutyCycle defaultDutyPercent[]{50.0f, 95.0f, 50.0f, 85.0f, 10.0f};

Esp32HardwarePwm pwm(pinList);

SimpleTimer procTimer;

void doPWM()
{
	// Five independent triangle-wave counters, pre-seeded 1/5th of a full cycle (400 ticks) apart.
	// Full cycle: 0→100→0 = 2000 ticks at 0.1%/tick.
	// Offsets: ch0=0↑, ch1=40↑, ch2=80↑, ch3=80↓, ch4=40↓
	static float pct[5] = {0.0f, 40.0f, 80.0f, 80.0f, 40.0f};
	static bool countUp[5] = {true, true, true, false, false};
	const float step = 0.5f;

	Serial << _F("duty: ");
	for(uint chan = 0; chan < pwm.getPinCount(); ++chan) {
		if(countUp[chan]) {
			pct[chan] += step;
			if(pct[chan] >= 100.0f) {
				pct[chan] = 100.0f;
				countUp[chan] = false;
			}
		} else {
			pct[chan] -= step;
			if(pct[chan] <= 0.0f) {
				pct[chan] = 0.0f;
				countUp[chan] = true;
			}
		}
		Serial << pct[chan] << _F("[") << chan << _F("] ");
		pwm.setDutyChanPercent(chan, pct[chan]);
	}
	Serial << endl;
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);
	if(!pwm.isInitialized()) {
		Serial << _F("Failed to initialize PWM") << endl;
		return;
	}

	// Change PWM frequency / period if required:
	// pwm.setFrequency(500);
	// pwm.setPeriod(2000); // microseconds

	debug_i("starting PWM demo with %d channels", pwm.getPinCount());
	Serial << _F("PWM period = ") << pwm.getPeriod() << _F("us, freq = ") << pwm.getFrequency() << _F(", resolution = ")
		   << pwm.getResolution() << _F(" bits, max duty = ") << pwm.getMaxDuty() << endl;

	// Set default duty on every channel
	for(uint8_t ch = 0; ch < pwm.getPinCount(); ++ch) {
		pwm.setDutyChanPercent(ch, defaultDutyPercent[ch]);
	}

	Serial << _F("PWM output set on all ") << pwm.getPinCount() << _F(" channels.") << endl
		   << _F("LED (pin ") << LED_PIN << _F(", channel ") << LED_CHANNEL << _F(") will sweep 0–100% in cycles.")
		   << endl;

	procTimer.initializeMs<10>(doPWM).start();
}

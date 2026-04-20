/*
 * SinePulse_HwPWM
 *
 * Six channels run a sinusoidal "breathing" pulse (period = 1 s) as a CYCLIC
 * queue of 50 × 20 ms linear micro-fades that approximate a raised-cosine
 * envelope:
 *
 *   duty(t) = PEAK × ½ × (1 − cos(2π t / T))
 *
 * All six channels run the same waveform but staggered by T/6 ≈ 167 ms, so
 * they appear as six evenly spaced phases of the same sine wave.
 *
 * Stagger implementation: each channel starts T/6 ms after the previous one
 * using a repeating timer.  Because they all cycle the identical queue
 * indefinitely, the phase relationship is maintained forever.
 *
 * Pins (adjust to suit your board):
 *   CH0 → GPIO 13
 *   CH1 → GPIO 12
 *   CH2 → GPIO 14
 *   CH3 → GPIO 27
 *   CH4 → GPIO 26
 *   CH5 → GPIO 25
 */

#include <SmingCore.h>
#include <Esp32HardwarePwm.h>
#include <cmath>

namespace
{
// ---------------------------------------------------------------------------
// Waveform config
// ---------------------------------------------------------------------------
constexpr uint32_t PERIOD_MS = 1000;				  ///< full sine period
constexpr uint32_t SEGMENT_MS = 20;					  ///< duration of each micro-fade
constexpr uint32_t SEGMENTS = PERIOD_MS / SEGMENT_MS; // 50
// Stagger is computed at runtime from pinList.size() so adding/removing
// pins automatically distributes phases evenly across the full period.
constexpr float PEAK_PCT = 100.0f;

// Pins — avoid GPIO 6-11 (flash), 34-39 (input-only on ESP32 classic).
std::vector<uint8_t> pinList{13, 12, 14, 27, 26, 25, 23};

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

SimpleTimer staggerTimer;

// ---------------------------------------------------------------------------
// startChannel: fill a SEGMENTS-entry CYCLIC queue with sine micro-fades.
//
// The raised-cosine envelope  b(t) = ½(1 − cos(2π t / T))  is sampled at
// the end of each SEGMENT_MS window:
//
//   target[i] = PEAK × ½ × (1 − cos(2π (i+1) / SEGMENTS))   i = 0…SEGMENTS-1
//
// The last target (i = SEGMENTS-1) evaluates to 0 (cos(2π) = 1), so the
// queue loops back to 0% with no discontinuity.
// ---------------------------------------------------------------------------
void startChannel(uint8_t ch)
{
	pwm.setQueueMode(ch, Esp32HardwarePwm::QueueMode::CYCLIC);
	pwm.setQueueCapacity(ch, SEGMENTS);
	for(uint32_t i = 0; i < SEGMENTS; i++) {
		float angle = 2.0f * float(M_PI) * float(i + 1) / float(SEGMENTS);
		float target = PEAK_PCT * 0.5f * (1.0f - cosf(angle));
		pwm.queueFadeChanCiePercent(ch, target, SEGMENT_MS);
	}
	pwm.startQueue(ch);
	Serial.printf("CH%u started\n", (unsigned)ch);
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(false);

	const uint32_t staggerMs = PERIOD_MS / pinList.size();

	Serial.println(_F("\nSinePulse_HwPWM"));
	Serial.printf("  Period:   %lu ms | Segments: %lu × %lu ms\n", (unsigned long)PERIOD_MS, (unsigned long)SEGMENTS,
				  (unsigned long)SEGMENT_MS);
	Serial.printf("  Channels: %u | Stagger: %lu ms | Peak: %.0f%%\n\n", (unsigned)pinList.size(),
				  (unsigned long)staggerMs, (double)PEAK_PCT);

	if(!pwm.isInitialized()) {
		Serial.println(_F("PWM init failed — check pin list"));
		return;
	}

	// CH0 starts immediately; remaining channels are launched staggerMs apart.
	startChannel(0);

	static uint8_t nextCh = 1;
	staggerTimer.initializeMs(staggerMs, []() {
		startChannel(nextCh++);
		if(nextCh >= pinList.size()) {
			staggerTimer.stop();
			Serial.println(_F("All channels running."));
		}
	});
	staggerTimer.start();
}

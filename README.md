# ESP32 Hardware PWM Component for Sming

A comprehensive C++ wrapper for the ESP32 LEDC PWM functionality, designed for the Sming framework.

## Features

- **Multiple PWM Instances**: Create multiple independent PWM instances with different configurations
- **Flexible Configuration**: Support for different frequencies, duty resolutions (1-20 bits), and speed modes
- **Phase Shifting**: Built-in support for phase shifting to reduce EMI
- **Spread Spectrum**: Optional spread-spectrum modulation of the base frequency to further reduce EMI
- **Hardware Fade**: Hardware-accelerated linear fade transitions via the ESP32 LEDC fade engine
- **Fade Queue**: Per-channel FIFO and CYCLIC fade queues with completion callbacks

## ESP32 LEDC Hardware Overview
```
 * the ESP32 PWM Hardware is much more powerful than the ESP8266, allowing wider PWM timers (up to 20 bit)
 * as well as much higher PWM frequencies (up to 40MHz for a 1 Bit wide PWM)
 * 
 * Overview:
 * +------------------------------------------------------------------------------------------------+
 * | LED_PWM                                                                                        |
 * |  +-------------------------------------------+   +-------------------------------------------+ |
 * |  | High_Speed_Channels¹                      |   | Low_Speed_Channels                        | |
 * |  |                   +-----+     +--------+  |   |                   +-----+     +--------+  | |
 * |  |                   |     | --> | h_ch 0 |  |   |                   |     | --> | l_ch 0 |  | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  | | h_timer 0 | --> |     |                 |   | | l_timer 0 | --> |     |                 | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 1 |  |   |                   |     | --> | l_ch 1 |  | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   |     |                 |   |                   |     |                 | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 2 |  |   |                   |     | --> | l_ch 2 |  | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  | | h_timer 1 | --> |     |                 |   | | l_timer 1 | --> |     |                 | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 3 |  |   |                   |     | --> | l_ch 3 |  | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   | MUX |                 |   |                   | MUX |                 | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 4 |  |   |                   |     | --> | l_ch 4 |  | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  | | h_timer 2 | --> |     |                 |   | | l_timer 2 | --> |     |                 | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 5 |  |   |                   |     | --> | l_ch 5 |  | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   |     |                 |   |                   |     |                 | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 6 |  |   |                   |     | --> | l_ch 6²|  | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  | | h_timer 3 | --> |     |                 |   | | l_timer 3 | --> |     |                 | |
 * |  | +-----------+     |     |     +--------+  |   | +-----------+     |     |     +--------+  | |
 * |  |                   |     | --> | h_ch 7 |  |   |                   |     | --> | l_ch 7²|  | |
 * |  |                   |     |     +--------+  |   |                   |     |     +--------+  | |
 * |  |                   +-----+                 |   |                   +-----+                 | |
 * |  +-------------------------------------------+   +-------------------------------------------+ |
 * +------------------------------------------------------------------------------------------------+
 * ¹ High speed channels are only available when SOC_LEDC_SUPPORT_HS_MODE is defined as 1
 * ² The ESP32C3 does only support six channels, so 6 and 7 are not available on that SoC
 * 
 * The nomenclature of timers in the high speed / low speed blocks is a bit misleading as the idf api 
 * speaks of "speed mode", which, to me, implies that this would be a mode configurable in a specific timer
 * while in reality, it does select a block of timers.
 * 
 * Maximum Timer width for PWM:
 * ============================
 * esp32   SOC_LEDC_TIMER_BIT_WIDE_NUM  (20)
 * esp32c3 SOC_LEDC_TIMER_BIT_WIDE_NUM  (14)
 * esp32s2 SOC_LEDC_TIMER_BIT_WIDE_NUM  (14)
 * esp32s3 SOC_LEDC_TIMER_BIT_WIDE_NUM  (14)
 * 
 * Number of Channels:
 * ===================
 * esp32   SOC_LEDC_CHANNEL_NUM         (8)
 * esp32c3 SOC_LEDC_CHANNEL_NUM         (6)
 * esp32s2 SOC_LEDC_CHANNEL_NUM         (8)
 * esp32s3 SOC_LEDC_CHANNEL_NUM 		(8)
 *
 * Some SoSs support a mode called HIGHSPEED_MODE which is essentially another full block of PWM hardware 
 * that adds SOC_LEDC_CHANNEL_NUM channels. 
 * Those Architectures have SOC_LEDC_SUPPORT_HS_MODE defined as 1.
 * In esp-idf-4.3 that's currently only the esp32 SOC 
 * 
 * Supports highspeed mode:
 * ========================
 * esp32 SOC_LEDC_SUPPORT_HS_MODE	(1)
 * 
 * hardware technical reference: 
 * =============================
 * https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf#ledpwm
 * 
 * Overview of the whole ledc-system here: 
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html
 * 
```
## Quick Start

### Basic Usage

```cpp
#include <SmingCore.h>
#include <Esp32HardwarePwm.h>

// Define pins for PWM output
std::vector<uint8_t> pwm_pins = {2, 4, 5, 18};

void init() {
    Serial.begin(SERIAL_BAUD_RATE);

    // Create PWM instance with default settings (10-bit, 1 kHz)
    Esp32HardwarePwm pwm(pwm_pins);

    if (pwm.isInitialized()) {
        pwm.setDutyChan(0, 256);              // raw duty on channel 0 (25 % of 1023)
        pwm.setDutyChan(1, 512);              // 50 % on channel 1
        pwm.analogWrite(pwm_pins[2], 768);    // legacy pin-indexed write

        Serial.println("PWM initialized successfully");
    } else {
        Serial.println("PWM initialization failed");
    }
}
```

### Advanced Configuration

```cpp
#include <SmingCore.h>
#include <Esp32HardwarePwm.h>

std::vector<uint8_t> led_pins = {12, 13, 14};

Esp32HardwarePwm pwm(led_pins, Esp32HardwarePwm::Config{
    .timer = {
        .resolution = LEDC_TIMER_12_BIT,  // 12-bit (0-4095)
        .frequency  = 20000,              // 20 kHz
    },
    .phaseShift = {
        .mode = Esp32HardwarePwm::PhaseShiftMode::AUTO,
    },
});

void init() {
    if (pwm.isInitialized()) {
        pwm.fadeToValueChan(0, pwm.getMaxDuty(), 2000);  // fade to 100% over 2 s
        pwm.fadeToPercentChan(1, 50.0f, 1500);           // fade to 50% over 1.5 s
    }
}
```

## API Reference

### Configuration

All configuration is passed through the nested `Esp32HardwarePwm::Config` aggregate:

```cpp
struct Esp32HardwarePwm::Config {
    ledc_channel_t channelStart = LEDC_CHANNEL_0;  // first LEDC channel to allocate
    TimerConfig        timer          = {};         // frequency, resolution, timer index
    PhaseShiftConfig   phaseShift     = {};         // OFF / AUTO / MANUAL
    SpreadSpectrumConfig spreadSpectrum = {};       // OFF / ON
};

struct TimerConfig {
    ledc_mode_t      speed_mode  = LEDC_LOW_SPEED_MODE;
    ledc_timer_bit_t resolution  = LEDC_TIMER_10_BIT;  // 1-20 bits
    ledc_timer_t     timer_num   = LEDC_TIMER_0;
    uint32_t         frequency   = 1000;               // Hz
    ledc_clk_cfg_t   clk_cfg     = LEDC_AUTO_CLK;
};

struct PhaseShiftConfig {
    PhaseShiftMode      mode            = PhaseShiftMode::OFF;
    std::vector<int>    manual_hpoints  = {};  // one per pin, MANUAL mode only
};

struct SpreadSpectrumConfig {
    SpreadSpectrumMode mode         = SpreadSpectrumMode::OFF;
    uint8_t            WidthPercent = 0;   // deviation as % of base frequency
    uint16_t           Subsampling  = 0;   // PWM cycles between updates
};
```

### Main Class: Esp32HardwarePwm

#### Constructors

At its most basic the constructor only needs a pin list; all other settings default to 10-bit / 1 kHz / `LEDC_TIMER_0` / low-speed mode.

```cpp
Esp32HardwarePwm(std::vector<uint8_t>& pins);
Esp32HardwarePwm(std::vector<uint8_t>& pins, const Config& config);
```

> **Multiple instances**: each instance must use a distinct set of LEDC channels.  Set `config.channelStart` on the second instance to the first free channel (e.g. `LEDC_CHANNEL_3` if the first instance uses three channels).  Instances that share the same `timer_num` also share the same frequency and resolution — changing one affects the other.

#### Duty Cycle Control

Channels are zero-indexed in the order the pins were supplied to the constructor.

##### Channel interface (preferred)
```cpp
bool     setDutyChan(uint8_t channel, uint32_t duty, bool update_immediately = true);
uint32_t getDutyChan(uint8_t channel);
bool     setPhaseShiftChan(uint8_t channel, uint32_t phase_shift, bool update_immediately = true);
```

##### Pin interface (legacy)
```cpp
bool     setDuty(uint8_t pin, uint32_t duty, bool update_immediately = true);
uint32_t getDuty(uint8_t pin);
bool     analogWrite(uint8_t pin, uint32_t duty);
```

#### Frequency and Period Control
These calls set the pwm frequency. Be aware that, if spread spectrum is enabled, this is the center frequency that the spectrum is spread around
```cpp
bool setFrequency(uint32_t frequency);
uint32_t getFrequency() const;
bool setPeriod(uint32_t period_us);
uint32_t getPeriod() const;
```

#### Information
This interface provides information about the current Esp32HardwarePwm instance
```cpp
uint32_t getMaxDuty() const;
uint8_t getResolution() const;
uint8_t getPinCount() const;
bool isInitialized() const;
```

#### Control Functions
```cpp
void update();
bool start(uint8_t pin);
bool stop(uint8_t pin, uint8_t idle_level = 0);
void startAll();
void stopAll(uint8_t idle_level = 0);
```

#### Hardware Fade

Fade support is installed automatically on first use.

```cpp
bool enableFade();    // install LEDC fade ISR (called automatically by fade methods)
void disableFade();

// Fade to an absolute duty value
bool fadeToValueChan(uint8_t channel, uint32_t target_duty, uint32_t fade_time_ms);
// Fade to a percentage (0.0 – 100.0)
bool fadeToPercentChan(uint8_t channel, float target_pct, uint32_t fade_time_ms);
// Returns true while a hardware fade is in progress
bool isFadingChan(uint8_t channel) const;
```

#### Fade Queue

Each channel has an independent fade queue that can chain multiple fades without
application polling.  The queue depth defaults to `FADE_QUEUE_DEPTH` (10) and can
be changed per-channel at runtime before the queue is filled.

##### Modes

| Mode | Behaviour | Auto-start |
|------|-----------|------------|
| `FIFO` (default) | Entries play once in order; `onQueueEmpty` fires when exhausted | Yes — playback starts on first `queueFadeChan()` call |
| `CYCLIC` | Entries loop endlessly back to entry 0; `onCyclicWrap` fires each loop | No — call `startQueue()` after seeding all entries |

##### Queue management

```cpp
void    setQueueMode(uint8_t channel, QueueMode mode);  // FIFO or CYCLIC
QueueMode getQueueMode(uint8_t channel) const;

// Override the auto-start default set by setQueueMode()
void setQueueAutoStart(uint8_t channel, bool autoStart);
bool getQueueAutoStart(uint8_t channel) const;

// Change queue depth (only while queue is empty; default FADE_QUEUE_DEPTH = 10)
bool     setQueueCapacity(uint8_t channel, uint16_t depth);
uint16_t getQueueCapacity(uint8_t channel) const;

uint16_t getQueueEntries(uint8_t channel) const;  // entries currently queued
void     resetQueue(uint8_t channel);            // clear queue, preserve capacity
```

##### Enqueueing and starting

```cpp
// Enqueue a fade (absolute duty or percentage)
bool queueFadeChan(uint8_t channel, uint32_t targetDuty, uint32_t fadeTimeMs);
bool queueFadePercentChan(uint8_t channel, float targetPct, uint32_t fadeTimeMs);

// Explicitly start a CYCLIC queue (or restart an idle FIFO queue)
bool startQueue(uint8_t channel);
```

##### Callbacks

```cpp
// Fires after every individual fade completes (any mode)
pwm.setOnFadeDoneCallback([](uint8_t ch) { ... });

// Fires when a FIFO queue drains to empty
pwm.setOnQueueEmptyCallback([](uint8_t ch) { ... });

// Fires each time a CYCLIC queue wraps back to entry 0
pwm.setOnCyclicWrapCallback([](uint8_t ch) { ... });
```

##### Example — CYCLIC queue

```cpp
pwm.setQueueMode(1, Esp32HardwarePwm::QueueMode::CYCLIC);
pwm.queueFadePercentChan(1, 100.0f, 1000);
pwm.queueFadePercentChan(1,   0.0f, 1000);
pwm.queueFadePercentChan(1,  50.0f, 1000);
pwm.startQueue(1);  // must be called after seeding; CYCLIC does not auto-start
```

See `samples/FadeQueue_HwPWM` for a complete demonstration of both modes.

## License

This library is provided under the LGPL v3 license as part of the Sming Framework Project.

## References

- [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/ledc.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf#ledpwm)
- [Sming Framework](https://github.com/SmingHub/Sming)

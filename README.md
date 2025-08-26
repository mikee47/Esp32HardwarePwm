# ESP32 Hardware PWM Component for Sming

A comprehensive C++ wrapper for the ESP32 LEDC PWM functionality, designed for the Sming framework.

## Features

- **Multiple PWM Instances**: Create multiple independent PWM instances with different configurations
- **Flexible Configuration**: Support for different frequencies, duty resolutions (1-20 bits), and speed modes
- **Phase Shifting**: Built-in support for phase shifting to reduce EMI
- (todo) **Hardware Fade**: Hardware-accelerated fade transitions

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
#include "Esp32HardwarePwm.h"

// Define pins for PWM output
std::vector<uint8_t> pwm_pins = {2, 4, 5, 18};

void init() {
    Serial.begin(SERIAL_BAUD_RATE);

    // Create PWM instance with default settings
    Esp32HardwarePwm pwm(pwm_pins);

    if (pwm.isInitialized()) {
        // Set different duty cycles
        pwm.setDutyChanPercent(0, 25.0f);   // 25% duty cycle on channel 0
        pwm.setDutyChan(1, 512);            // Duty cycle on channel 1
        pwm.analogWrite(2, 768);            // Duty cycle on channel 2

        Serial.println("PWM initialized successfully");
    } else {
        Serial.println("PWM initialization failed");
    }
}
```

### Advanced Configuration

```cpp
#include "Esp32HardwarePwm.h"

void advancedPwmExample() {
    std::vector<uint8_t> led_pins = {12, 13, 14};

    Esp32HwPwmConfig config;
    config.frequency = 20000;                    // 20kHz
    config.resolution = LEDC_TIMER_12_BIT;       // 12-bit resolution
    config.speed_mode = LEDC_LOW_SPEED_MODE;     // Low speed mode
    config.phaseShift.mode = PhaseShiftMode::AUTO; // Enable phase shifting
    config.spreadSpectrum.mode = SpreadSpectrumMode::OFF; // No spread spectrum

    Esp32HardwarePwm pwm(led_pins, config);

    if (pwm.isInitialized()) {
        pwm.enableFade();

        pwm.fadeToPercent(0, 100.0f, 2000);   // Fade to 100% over 2 seconds
        pwm.fadeToPercent(1, 50.0f, 1500);    // Fade to 50% over 1.5 seconds
        pwm.fadeToValue(2, 2048, 1000, true); // Fade to value over 1 second, wait for completion
    }
}
```

## API Reference

### Configuration Structures

```cpp
struct Esp32HwPwmTimerConfig {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t resolution;
    ledc_timer_t timer_num;
    uint32_t frequency;
    ledc_clk_cfg_t clk_cfg;
};

struct Esp32HwPwmSpreadSpectrumConfig {
    SpreadSpectrumMode mode;
    int WidthPercent;
    int Subsampling;
    int StepsizeHz;
};

struct Esp32HwPwmPhaseShiftConfig {
    PhaseShiftMode mode;
    std::vector<uint32_t> manual_hpoints;
};
```

### Main Class: Esp32HardwarePwm

#### Constructors
At it's most basic, the Esp32 pwm can be instantiated using just the pins array. In this case, it will 
behave much like the original HardwarePWM implementation in Sming, with the minor difference that inctead
of a C style array, the constructor teakes a std::vector (todo: should there be an overload with a C array?)
the `Esp32HwPwmConfig` structure comes with additional settings to configure
phase shift, the timer and spread spectrum settings.
*Caution:* this library does not provide internal resource allocation. If you don't provide a `Esp32HwPwmConfig` structure,
all resource allocations will be as per default, specifically, for the timer, those are
- `timer.speed_mode` = `LEDC_LOW_SPEED_MODE` - available on all Esp32 variants 
- `timer.timer_num`  = `LEDC_TIMER_0` - the first timer in the system
- `timer.resolution` = `LEDC_TIMER_10_BIT` - a 10 Bit timer (max duty=1023)
- `timer.frequency`  = 1000 - 1kHz
- `timer.clk_cfg`    = `LEDC_AUTO_CLK`
those are all timer specific settings and are generally a good base setting. If you want more than one `Esp32HardwarePwm` instance in 
your code, you *can* use the same timer settings - meaning both instances will share the same timer - no problem there, but they will
share the same settings and if you change the timer settings in one (such as the frequency) that will also change for the other.
If you are using multiple instances, you will at least have to set the `channelStart` value on the 2nd instance, since otherwise, it 
will be set to LEDC_CHANNEL_0, overwriting the channel config in your first instance. 
On an embedded platform, it seems reasonable to leave full control over the hardware allocation to the developer rather than automatically
allocate timers and channels from a pool, but this may be a pitfal.
So: if you use more than one pwm object, make sure that you instantiate the 2nd one with a minimal `Esp23HwPwmConfig.channelStart` set to
the first free channel on your system.
Also be aware that the `timer.speed_mode` devides that channel groups in two and one `Esp32HardwarePwm` instance cannot overlap between the two.
If you have one instance using five channels and you want to create a 2nd instance with four channels on the same `timer.speed_mode` you will get a runtime error, since the maximum amount of channels is 8 per speed mode (depending on the SoC, only the Esp32 has high speed timers, and the Esp32c3, as an example, has only six channels and a low speed timer).
As said: channel allocation is left to the developer!

##### Phase Shift
when building a high power driver for LEDs or a motor, it might be desireable to not have all channels switch on at the exact same time. Phase shifting helps by allowing the developer to set a per-channel delay within the pwm period.
The easiest way is to set `Esp32HwPwmConfig.phaseShift.mode = PhaseShiftMode::AUTO` which will make sure that the phases are equally staggered across the pwm period.
You can also set `Esp32HwPwmConfig.phaseShift.mode = PhaseShiftMode::MANUAL` in wich case you have to provide a `std::vector` of size pins of int values between 0 and pwm period as `Esp32HwPwmConfig.phaseShift.manual_hpoints` - those will then be used as the hpoints for your signals. This way, you could, as an example, stagger them by 10% of your perio, starting channel 0 at t=0, channel 1 at t=10%, channel 2 at 20% etc. You will have to calculate those hpoint values manually for any given pwm frequency / period.
It is generally suggested to leave phaseShift `OFF` in low current uses and `AUTO` where the switchim impact on the power lines is significant or EMI is a consideration.

##### Spread Spectrum


```cpp
Esp32HardwarePwm(std::vector<uint8_t>& pins);
Esp32HardwarePwm(std::vector<uint8_t>& pins, const Esp32HwPwmConfig& config);
```

#### Destructor
```cpp
virtual ~Esp32HardwarePwm();
```

#### Duty Cycle Control
The library includes two different ways to access a pwm channel - by `pin` or `channel`.
The per pin interface seems a bit more straight forward for makers who come from the hardware side
while the per channel interface is the native interface for the ledc_ api. 


#####per channel interface
```cpp
uint32_t getDutyChan(uint8_t channel);
bool setDutyChan(uint8_t channel, uint32_t duty, bool update_immediately = true);
bool setDutyChanPercent(uint8_t channel, float percentage, bool update_immediately = true);
float getDutyChanPercent(uint8_t channel);
```

#####per pin interface
```cpp
bool setDuty(uint8_t pin, uint32_t duty, bool update_immediately = true);
uint32_t getDuty(uint8_t pin);
bool analogWrite(uint8_t pin, uint32_t duty);
```

#### Phase Shift Control
```cpp
bool setPhaseShiftChan(uint8_t pin, uint32_t phase_shift, bool update_immediately = true);
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
```cpp
bool enableFade();
void disableFade();
bool fadeToValue(uint8_t pin, uint32_t target_duty, uint32_t fade_time_ms, bool wait_for_completion = false);
bool fadeToPercent(uint8_t pin, float target_percent, uint32_t fade_time_ms, bool wait_for_completion = false);
```

## License

This library is provided under the LGPL v3 license as part of the Sming Framework Project.

## References

- [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/ledc.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf#ledpwm)
- [Sming Framework](https://github.com/SmingHub/Sming)

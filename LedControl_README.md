# ESP32 LEDC Control Library

A high-performance, low-level ESP32 LEDC PWM control library for the Sming framework.

## Overview

This library provides direct access to the ESP32 LEDC (LED Control) peripheral with minimal overhead and maximum flexibility. It follows Sming's philosophy of staying "close to the metal" while providing a clean C++ interface.

## Key Features

- **Zero FreeRTOS Dependencies**: Uses Sming's single-threaded model
- **Minimal Overhead**: Simple inline functions for maximum performance
- **Hardware Resource Management**: Automatic timer and channel allocation
- **Type Safety**: Separate HSTimer/LSTimer classes for high/low speed modes
- **Platform Compatibility**: Supports all ESP32 variants
- **Clean API**: Modern C++ design following Sming conventions

## Hardware Capabilities

| SoC     | Low Speed Channels | High Speed Channels | Timers per Mode | Max Resolution |
|---------|-------------------|-------------------|----------------|---------------|
| ESP32   | 8                 | 8                 | 4              | 20 bits       |
| ESP32-C3| 6                 | 0                 | 4              | 14 bits       |
| ESP32-S2| 8                 | 0                 | 4              | 14 bits       |
| ESP32-S3| 8                 | 0                 | 4              | 14 bits       |

## Quick Start

```cpp
#include <LedControl.h>

// Create timer and channels
LEDC::LSTimer timer(1000, LEDC_TIMER_10_BIT); // 1kHz, 10-bit resolution
LEDC::Channel led1(timer);
LEDC::Channel led2(timer);

void init() {
    // Configure channels
    led1.configure({
        .gpio = 2,
        .duty = 512,  // 50% duty cycle
    });
    
    led2.configure({
        .gpio = 4,
        .duty = 256,  // 25% duty cycle
    });
    
    // Control LEDs
    led1.setDuty(768);           // 75% duty cycle
    led2.setDutyPercent(10.0f);  // 10% duty cycle
}
```

## Advanced Usage

```cpp
#include <LedControl.h>

// Multiple timers for different frequencies
LEDC::LSTimer ledTimer(5000, LEDC_TIMER_12_BIT);     // 5kHz for LEDs
LEDC::LSTimer servoTimer(50, LEDC_TIMER_16_BIT);     // 50Hz for servos

// RGB LED control
LEDC::Channel redLed(ledTimer);
LEDC::Channel greenLed(ledTimer);
LEDC::Channel blueLed(ledTimer);

// Servo control
LEDC::Channel panServo(servoTimer);
LEDC::Channel tiltServo(servoTimer);

void init() {
    // Configure RGB LEDs
    redLed.configure({.gpio = 25, .duty = 0});
    greenLed.configure({.gpio = 26, .duty = 0});
    blueLed.configure({.gpio = 27, .duty = 0});
    
    // Configure servos (1.5ms center position)
    uint32_t centerDuty = (1500 * servoTimer.getMaxDuty()) / 20000; // 1.5ms of 20ms
    panServo.configure({.gpio = 16, .duty = centerDuty});
    tiltServo.configure({.gpio = 17, .duty = centerDuty});
    
    // Set purple color (red + blue)
    setRgbColor(255, 0, 255);
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    redLed.setDuty((r * ledTimer.getMaxDuty()) / 255);
    greenLed.setDuty((g * ledTimer.getMaxDuty()) / 255);
    blueLed.setDuty((b * ledTimer.getMaxDuty()) / 255);
}
```

## API Reference

### Timer Classes

#### LEDC::LSTimer (Low Speed Timer)
```cpp
// Constructors
LSTimer(uint32_t frequency, ledc_timer_bit_t resolution);
LSTimer(uint32_t frequency, ledc_timer_bit_t resolution, ledc_clk_cfg_t clockSource);

// Methods
bool setFrequency(uint32_t frequency);
uint32_t getFrequency() const;
uint32_t getMaxDuty() const;
ledc_timer_bit_t getResolution() const;
operator bool() const;  // Check if timer is valid
```

#### LEDC::HSTimer (High Speed Timer - ESP32 only)
```cpp
// Same interface as LSTimer but uses high-speed mode
```

### Channel Class

#### LEDC::Channel
```cpp
// Constructor
Channel(TimerBase& timer);

// Configuration
struct Config {
    uint8_t gpio;
    uint32_t duty = 0;
    uint32_t hpoint = 0;
};

bool configure(const Config& config);

// Duty cycle control
bool setDuty(uint32_t duty);
uint32_t getDuty() const;
bool setDutyPercent(float percent);
float getDutyPercent() const;

// Control
bool start();
bool stop(uint8_t idleLevel = 0);
operator bool() const;  // Check if channel is valid
```

## Resource Management

The library automatically manages LEDC timer and channel allocation:

- Timers are allocated sequentially within each speed mode
- Channels are allocated from the appropriate pool based on timer type
- Failed allocations result in invalid objects (check with `operator bool()`)
- Resources are automatically freed when objects are destroyed

## Performance Considerations

- **Minimal Overhead**: Direct register access where possible
- **Inline Functions**: Critical paths are inlined for optimal performance
- **No Dynamic Allocation**: All resources managed statically
- **Single-Threaded**: No locking overhead

## Migration from HardwarePWM

```cpp
// Old HardwarePWM approach
HardwarePWM pwm(pins, pinCount);
pwm.analogWrite(pin, duty);

// New LEDC approach
LEDC::LSTimer timer(1000, LEDC_TIMER_10_BIT);
LEDC::Channel channel(timer);
channel.configure({.gpio = pin, .duty = duty});
```

## Examples

See the `examples/` directory for comprehensive usage examples:

- `basic_led_control.cpp` - Simple LED dimming
- `rgb_led_effects.cpp` - RGB LED color mixing
- `servo_control.cpp` - Servo motor positioning
- `multi_timer_demo.cpp` - Multiple timers with different configurations

## Compatibility

- **Sming Framework**: v4.7+
- **ESP-IDF**: v4.4+
- **Compiler**: C++17 standard required
- **Platforms**: All ESP32 variants

## License

LGPL v3 - Part of the Sming Framework Project

# ESP32 Hardware PWM Component for Sming

A comprehensive C++ wrapper for the ESP32 LEDC PWM functionality, designed for the Sming framework.

## Features

- **Multiple PWM Instances**: Create multiple independent PWM instances with different configurations
- **Automatic Resource Management**: Global resource manager prevents channel and timer conflicts
- **Thread-Safe Operations**: Safe for use in multi-threaded applications
- **Flexible Configuration**: Support for different frequencies, duty resolutions (1-20 bits), and speed modes
- **Phase Shifting**: Built-in support for phase shifting to reduce EMI
- **Hardware Fade**: Hardware-accelerated fade transitions
- **High Performance**: Direct ESP-IDF LEDC API integration
- **Memory Efficient**: RAII-based resource management

## ESP32 LEDC Hardware Overview

The ESP32 LEDC peripheral provides up to 16 PWM channels:
- **Low Speed Mode**: 8 channels (always available)
- **High Speed Mode**: 8 additional channels (ESP32 only)

Each group has 4 timers that can be shared between channels. The wrapper automatically manages timer allocation and sharing when channels have compatible configurations.

## Quick Start

### Basic Usage

```cpp
#include <SmingCore.h>
#include "Esp32HardwarePwm.h"

// Define pins for PWM output
uint8_t pwm_pins[] = {2, 4, 5, 18};

void init() {
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Create PWM instance with default settings (1kHz, 10-bit resolution)
    auto pwm = std::make_unique<Esp32HardwarePwm>(pwm_pins, 4);
    
    if (pwm->isInitialized()) {
        // Set different duty cycles
        pwm->setDutyPercent(2, 25.0f);   // 25% duty cycle on pin 2
        pwm->setDuty(4, 512);            // 50% duty cycle on pin 4 (512/1024)
        pwm->analogWrite(5, 768);        // 75% duty cycle on pin 5
        
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
    uint8_t led_pins[] = {12, 13, 14};
    
    // Configure for high-frequency PWM with phase shifting
    Esp32PwmConfig config;
    config.frequency = 20000;                    // 20kHz
    config.resolution = LEDC_TIMER_12_BIT;       // 12-bit resolution (0-4095)
    config.speed_mode = LEDC_LOW_SPEED_MODE;     // Low speed mode
    config.use_phase_shift = true;               // Enable phase shifting for EMI reduction
    config.enable_fade = true;                   // Enable hardware fade
    
    auto pwm = std::make_unique<Esp32HardwarePwm>(led_pins, 3, config);
    
    if (pwm->isInitialized()) {
        // Enable fade functionality
        pwm->enableFade();
        
        // Start smooth fade transitions
        pwm->fadeToPercent(12, 100.0f, 2000);   // Fade to 100% over 2 seconds
        pwm->fadeToPercent(13, 50.0f, 1500);    // Fade to 50% over 1.5 seconds
        pwm->fadeToValue(14, 2048, 1000, true); // Fade to 50% over 1 second, wait for completion
    }
}
```

### Multiple Independent PWM Instances

```cpp
void multipleInstancesExample() {
    // RGB LED with high frequency
    uint8_t rgb_pins[] = {16, 17, 18};
    Esp32PwmConfig rgb_config;
    rgb_config.frequency = 10000;
    rgb_config.resolution = LEDC_TIMER_8_BIT;
    rgb_config.use_phase_shift = true;
    
    auto rgb_pwm = std::make_unique<Esp32HardwarePwm>(rgb_pins, 3, rgb_config);
    
    // Servo control with standard frequency
    uint8_t servo_pins[] = {19, 21};
    Esp32PwmConfig servo_config;
    servo_config.frequency = 50;  // 50Hz for servo control
    servo_config.resolution = LEDC_TIMER_16_BIT;
    
    auto servo_pwm = std::make_unique<Esp32HardwarePwm>(servo_pins, 2, servo_config);
    
    // Both instances work independently
    if (rgb_pwm->isInitialized() && servo_pwm->isInitialized()) {
        // Control RGB LED
        rgb_pwm->setDutyPercent(16, 100.0f);  // Red
        rgb_pwm->setDutyPercent(17, 0.0f);    // Green
        rgb_pwm->setDutyPercent(18, 50.0f);   // Blue
        
        // Control servos (assuming 1.5ms = center position)
        uint32_t center_position = (1.5f / 20.0f) * rgb_pwm->getMaxDuty(); // 1.5ms out of 20ms
        servo_pwm->setDuty(19, center_position);
        servo_pwm->setDuty(21, center_position);
    }
}
```

## API Reference

### Configuration Structure

```cpp
struct Esp32PwmConfig {
    uint32_t frequency = 1000;                          // PWM frequency in Hz
    ledc_timer_bit_t resolution = LEDC_TIMER_10_BIT;    // Duty resolution (1-20 bits)
    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;       // Speed mode
    ledc_clk_cfg_t clock_source = LEDC_AUTO_CLK;       // Clock source
    bool use_phase_shift = false;                       // Enable phase shifting
    bool enable_fade = false;                          // Enable hardware fade
};
```

### Main Class Methods

#### Constructor
```cpp
Esp32HardwarePwm(const uint8_t* pins, uint8_t pin_count);
Esp32HardwarePwm(const uint8_t* pins, uint8_t pin_count, const Esp32PwmConfig& config);
```

#### Duty Cycle Control
```cpp
bool setDuty(uint8_t pin, uint32_t duty, bool update_immediately = true);
uint32_t getDuty(uint8_t pin) const;
bool setDutyPercent(uint8_t pin, float percentage, bool update_immediately = true);
float getDutyPercent(uint8_t pin) const;
bool analogWrite(uint8_t pin, uint32_t duty);  // Arduino-style interface
```

#### Frequency Control
```cpp
bool setFrequency(uint32_t frequency);
uint32_t getFrequency() const;
bool setPeriod(uint32_t period_us);
uint32_t getPeriod() const;
```

#### Control Functions
```cpp
bool start(uint8_t pin);
bool stop(uint8_t pin, uint8_t idle_level = 0);
void startAll();
void stopAll(uint8_t idle_level = 0);
void update();  // Apply pending changes
```

#### Information
```cpp
uint32_t getMaxDuty() const;
uint8_t getResolution() const;
uint8_t getChannelCount() const;
bool isInitialized() const;
const PwmChannelInfo* getChannelInfo(uint8_t pin) const;
```

#### Hardware Fade
```cpp
bool enableFade();
void disableFade();
bool fadeToValue(uint8_t pin, uint32_t target_duty, uint32_t fade_time_ms, 
                 bool wait_for_completion = false);
bool fadeToPercent(uint8_t pin, float target_percent, uint32_t fade_time_ms,
                   bool wait_for_completion = false);
```

## Resource Management

The library includes a global resource manager (`Esp32PwmResourceManager`) that:

- Tracks allocation of all LEDC channels and timers
- Prevents resource conflicts between multiple PWM instances
- Automatically shares timers between channels with compatible configurations
- Provides thread-safe resource allocation and deallocation

### Available Resources by SoC

| SoC     | Low Speed Channels | High Speed Channels | Timers per Mode | Max Resolution |
|---------|-------------------|-------------------|----------------|---------------|
| ESP32   | 8                 | 8                 | 4              | 20 bit        |
| ESP32-C3| 6                 | 0                 | 4              | 14 bit        |
| ESP32-S2| 8                 | 0                 | 4              | 14 bit        |
| ESP32-S3| 8                 | 0                 | 4              | 14 bit        |

## Performance Considerations

### Frequency vs Resolution Trade-off

Higher frequencies require lower duty resolution:
- 40 MHz max frequency with 1-bit resolution
- ~1 kHz typical frequency with 20-bit resolution
- The library automatically validates frequency/resolution combinations

### Timer Sharing

Channels using the same timer configuration (frequency, resolution, clock source) automatically share timers, maximizing resource efficiency.

### Phase Shifting

When enabled, phase shifting distributes the rising edges of PWM signals evenly across the period, reducing:
- Peak current draw
- Electromagnetic interference (EMI)
- Power supply noise

## Thread Safety

All public methods are thread-safe using internal mutexes. The resource manager also uses locks to prevent race conditions during resource allocation.

## Error Handling

The library provides comprehensive error handling:
- Initialization validates pin counts and resource availability
- Duty cycle values are automatically clamped to valid ranges
- ESP-IDF error codes are logged with descriptive messages
- Methods return boolean success indicators where appropriate

## Migration from ESP8266

The API maintains compatibility with ESP8266 PWM libraries while offering ESP32-specific features:

```cpp
// ESP8266 style (still works)
pwm.analogWrite(pin, duty);

// ESP32 enhanced features
pwm.setDutyPercent(pin, 75.0f);
pwm.fadeToPercent(pin, 25.0f, 2000);
```

## Examples

The library includes comprehensive examples demonstrating various use cases:

### Basic Examples
- **`basic_pwm_example.cpp`** - Simple PWM control with LED dimming
- **`advanced_pwm_demo.cpp`** - Multiple instances with different configurations
- **`test_suite.cpp`** - Comprehensive testing and validation

### Application Examples
- **`servo_control_example.cpp`** - Servo motor control with smooth positioning
  - Pan/tilt servo control
  - Angle-to-pulse-width conversion
  - Smooth motion with acceleration
  - Serial command interface

- **`rgb_led_effects_example.cpp`** - RGB LED effects and color mixing
  - HSV to RGB color space conversion
  - Multiple effect patterns (rainbow, breathing, fire, ocean)
  - Smooth color transitions
  - Interactive control interface

- **`motor_control_example.cpp`** - DC motor speed control
  - H-bridge motor driver support
  - Acceleration and deceleration profiles
  - Direction control and emergency stop
  - Multiple motor coordination

- **`integration_example.cpp`** - Complete IoT application
  - Multiple PWM subsystems (lighting, servos, fans, buzzer)
  - Web interface for remote control
  - MQTT integration for IoT connectivity
  - JSON configuration and status reporting
  - Temperature-based automatic fan control

### Building Examples

Each example can be built as a standalone Sming application:

```bash
# Navigate to your Sming application directory
cd examples/servo_control
make flash  # Build and flash the servo control example

# For advanced examples that require network connectivity
cd examples/integration
# Edit the source to configure WiFi credentials
make flash
```

## Performance Considerations

### Resource Usage
- Each PWM instance uses approximately 200-400 bytes of RAM
- Resource manager adds ~100 bytes overhead
- Channel allocation is O(1) operation
- Timer sharing reduces overall resource usage

### Timing Accuracy
- Hardware PWM provides microsecond precision
- Frequency range: 1 Hz to 40 MHz (depending on resolution)
- Phase shift accuracy: ±1 PWM clock cycle
- Hardware fade provides smooth transitions without CPU intervention

### Best Practices
1. **Initialize once**: Create PWM instances during setup, not in loops
2. **Batch updates**: Use `update()` to apply multiple changes simultaneously
3. **Share timers**: Use compatible frequencies to allow timer sharing
4. **Use appropriate resolution**: Higher resolution reduces max frequency
5. **Enable phase shift**: Reduces EMI and power supply noise

## License

This library is provided under the LGPL v3 license as part of the Sming Framework Project.

## Contributing

Contributions are welcome! Please ensure:
- Code follows the existing style
- New features include appropriate documentation
- Thread safety is maintained
- Resource management is properly handled
- Include appropriate examples for new features

## References

- [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/ledc.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf#ledpwm)
- [Sming Framework](https://github.com/SmingHub/Sming)

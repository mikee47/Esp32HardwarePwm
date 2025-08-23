# Changelog

All notable changes to the ESP32 LEDC Control Library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2025-06-12

### BREAKING CHANGES
- **Complete API Redesign**: Restructured based on Sming maintainer feedback (PR #2624)
- **Library Separation**: Moved from HardwarePWM modification to standalone LedControl library
- **Namespace Introduction**: All classes now in LEDC namespace
- **Timer Class Split**: Separate HSTimer/LSTimer classes for type safety

### Added
- **New Library Structure**
  - Standalone `LedControl` library following Sming conventions
  - LEDC namespace organization for clean API separation
  - Type-safe HSTimer and LSTimer classes
  - Separate timer instances for different speed modes

- **Simplified Resource Management**
  - Removed FreeRTOS dependencies (mutexes, tasks)
  - Single-threaded resource allocation following Sming philosophy
  - Lightweight ResourceManager singleton
  - Automatic cleanup with RAII pattern

- **Performance Optimizations**
  - Inline functions for critical paths
  - Direct register access where possible
  - Minimal overhead design
  - Zero dynamic allocation

- **Enhanced API Design**
  - Modern C++ with RAII and move semantics
  - Clean configuration structures
  - Utility functions for common operations (servo control, RGB conversion)
  - Comprehensive error checking with bool operators

- **Sming Framework Integration**
  - Follows Sming coding conventions (CamelCase types, camelCase methods)
  - Compatible with Sming's single-threaded model
  - Minimal external dependencies
  - Clean separation from ESP-IDF high-level APIs

### Changed
- **Code Style**: Updated to follow Sming conventions
  - CamelCase for types and classes
  - camelCase for methods and variables
  - Consistent naming throughout

- **Architecture**: Simplified based on maintainer feedback
  - Removed complex thread-safety mechanisms
  - Eliminated FreeRTOS dependencies
  - Streamlined resource management
  - Direct hardware access approach

- **API Surface**: Cleaner, more focused interface
  - Removed unnecessary complexity
  - Better separation of concerns
  - Type-safe timer selection
  - Consistent method naming

### Removed
- **Thread Safety**: Removed mutex-based protection (not needed in Sming)
- **FreeRTOS Dependencies**: Eliminated all FreeRTOS-specific code
- **Complex Abstractions**: Simplified resource management
- **Unused Features**: Removed features not aligned with Sming philosophy

### Examples Restructured
- **New LEDC-based Examples**
  - `basic_led_control.cpp` - Simple LED brightness control
  - `rgb_led_effects.cpp` - RGB LED effects with HSV color space
  - `multi_timer_demo.cpp` - Multiple timer usage demonstration

- **Utility Functions**
  - HSV to RGB color space conversion
  - Servo angle to duty cycle conversion
  - RGB value to duty cycle conversion

### Documentation Updated
- **New Library Documentation** - Comprehensive guide for LedControl library
- **API Reference** - Updated for new LEDC namespace structure
- **Migration Guide** - Instructions for moving from HardwarePWM
- **Performance Notes** - Sming-specific optimization information

### Build System
- **Component Structure** - Proper Sming library organization
- **Makefile Updates** - Sming-compatible build configuration
- **Dependencies** - Minimal ESP-IDF component requirements

## [1.0.0] - 2025-06-12 [DEPRECATED]

### Note
This version represented the initial comprehensive implementation but has been
superseded by v2.0.0 which incorporates feedback from Sming maintainers.

### Added
- Complete C++ wrapper for ESP-IDF LEDC PWM API
- Multiple PWM instances with independent configurations
- Hardware resource management with automatic allocation/deallocation
- Thread-safe operations with mutex protection (removed in v2.0.0)
- Hardware fade transitions and phase shifting
- Platform compatibility for all ESP32 variants
- Comprehensive examples and documentation

---

## Migration Guide v1.0.0 → v2.0.0

### Key Changes

**Old API (v1.0.0):**
```cpp
#include "Esp32HardwarePwm.h"

uint8_t pins[] = {2, 4, 5};
Esp32HardwarePwm pwm(pins, 3, 1000, LEDC_TIMER_10_BIT);
pwm.setDutyPercent(2, 50.0f);
```

**New API (v2.0.0):**
```cpp
#include <LedControl.h>

LEDC::LSTimer timer(1000, LEDC_TIMER_10_BIT);
LEDC::Channel led1(timer);
LEDC::Channel led2(timer);
LEDC::Channel led3(timer);

led1.configure({.gpio = 2});
led1.setDutyPercent(50.0f);
```

### Benefits of v2.0.0
- **Cleaner API**: More intuitive timer/channel relationship
- **Better Performance**: No mutex overhead, inline optimizations
- **Type Safety**: Separate HSTimer/LSTimer classes
- **Sming Integration**: Follows framework conventions
- **Easier Resource Management**: Automatic timer sharing

---

## Contributing

When contributing to this project, please:
1. Follow Sming coding conventions (CamelCase types, camelCase methods)
2. Maintain single-threaded design philosophy
3. Avoid FreeRTOS dependencies
4. Keep close to hardware level
5. Add appropriate examples for new functionality
6. Update documentation for API changes

## License

This project is licensed under the LGPL v3 License - see the LICENSE file for details.

## [1.0.0] - 2025-06-12

### Added
- **Core PWM Functionality**
  - Complete C++ wrapper for ESP-IDF LEDC PWM API
  - Support for all ESP32 variants (ESP32, ESP32-C3, ESP32-S2, ESP32-S3)
  - Multiple PWM instances with independent configurations
  - Hardware resource management with automatic allocation/deallocation
  - Thread-safe operations with mutex protection

- **Advanced Features**
  - Hardware fade transitions with configurable timing
  - Phase shifting for EMI reduction and power optimization
  - Automatic timer sharing for compatible channel configurations
  - Support for 1-20 bit duty cycle resolution
  - Frequency range from 1Hz to 40MHz (resolution dependent)

- **Resource Management**
  - Global resource manager singleton (`Esp32PwmResourceManager`)
  - Automatic prevention of channel and timer conflicts
  - RAII-based resource cleanup
  - Resource usage tracking and reporting

- **Platform Compatibility**
  - Platform-specific feature detection and configuration
  - Compatibility macros for different ESP32 variants
  - Runtime SoC detection and capability adjustment

- **API Design**
  - Arduino-compatible interface (`analogWrite()`)
  - Percentage-based duty cycle control
  - Comprehensive error handling and validation
  - Detailed status and configuration reporting

- **Sming Framework Integration**
  - Complete Sming component structure
  - Component.json with proper metadata and dependencies
  - CMake and Make build system support
  - Kconfig configuration options
  - ESP-IDF component integration

### Examples Added
- **Basic Examples**
  - `basic_pwm_example.cpp` - Simple LED dimming and PWM basics
  - `advanced_pwm_demo.cpp` - Multi-instance usage and advanced features
  - `test_suite.cpp` - Comprehensive testing and validation suite

- **Application Examples**
  - `servo_control_example.cpp` - Servo motor positioning with smooth motion
  - `rgb_led_effects_example.cpp` - RGB LED effects with HSV color space
  - `motor_control_example.cpp` - DC motor speed control with H-bridge support
  - `integration_example.cpp` - Complete IoT application with web interface

### Documentation
- Comprehensive README with API reference and usage examples
- Detailed inline code documentation with Doxygen support
- Platform compatibility guide
- Performance considerations and best practices
- Migration guide from ESP8266 PWM libraries

### Build System
- Makefile with development and validation targets
- Doxygen configuration for documentation generation
- Syntax checking and validation tools
- Component packaging and installation support

### Features by Category

#### PWM Control
- Duty cycle control (raw values and percentages)
- Frequency and period control
- Individual channel start/stop control
- Batch updates for synchronized changes
- Hardware fade with configurable timing

#### Resource Management
- Automatic LEDC channel allocation
- Timer sharing optimization
- Thread-safe resource operations
- Resource usage reporting
- Cleanup on destruction

#### Hardware Support
- ESP32: 16 channels (8 low-speed + 8 high-speed)
- ESP32-C3: 6 channels (low-speed only)
- ESP32-S2: 8 channels (low-speed only)
- ESP32-S3: 8 channels (low-speed only)
- Runtime capability detection

#### Configuration Options
- Configurable PWM frequency (1Hz - 40MHz)
- Duty cycle resolution (1-20 bits)
- Speed mode selection (high/low speed)
- Clock source configuration
- Phase shift enable/disable
- Hardware fade enable/disable

### Technical Specifications
- **Memory Usage**: ~200-400 bytes per PWM instance
- **Thread Safety**: Full mutex protection for all operations
- **Resource Overhead**: ~100 bytes for global resource manager
- **Performance**: O(1) channel allocation, microsecond precision
- **Compatibility**: C++17 standard required

### Dependencies
- ESP-IDF v4.4+
- Sming Framework v4.7+
- FreeRTOS (included with ESP-IDF)
- ESP32 hardware platform

### Known Limitations
- High-speed mode only available on original ESP32
- Maximum 4 timers per speed mode
- Timer sharing requires compatible configurations
- Hardware fade not available on all ESP32 variants

### Testing
- Comprehensive test suite covering all major functionality
- Resource management validation
- Multi-instance testing
- Error condition handling
- Performance benchmarking

## [Unreleased]

### Planned Features
- Advanced waveform generation (sawtooth, triangle, custom)
- PWM input capture functionality
- Interrupt-based callbacks for fade completion
- Power management integration
- Additional platform support (ESP32-H2, ESP32-P4)

### Potential Improvements
- Memory usage optimization
- Additional color space conversions (RGB to HSL, etc.)
- Web-based configuration interface
- Real-time performance monitoring
- Advanced motor control algorithms

---

## Development Notes

### Design Decisions
1. **Singleton Resource Manager**: Chosen to prevent resource conflicts across multiple PWM instances
2. **RAII Pattern**: Ensures automatic resource cleanup and exception safety
3. **Thread Safety**: Comprehensive mutex protection for multi-threaded environments
4. **Configuration Structure**: Separate configuration object for flexibility and clarity
5. **Platform Abstraction**: Macro-based approach for compile-time optimization

### Performance Considerations
- Resource allocation is optimized for O(1) operations
- Timer sharing reduces overall resource usage
- Hardware fade offloads CPU for smooth transitions
- Batch updates minimize ESP-IDF API calls

### Code Quality
- C++17 standard compliance
- Comprehensive error handling
- Extensive documentation
- Memory leak prevention
- Resource leak prevention

### Testing Strategy
- Unit tests for core functionality
- Integration tests for multi-instance usage
- Hardware-in-the-loop testing on actual ESP32 devices
- Performance benchmarking
- Memory usage validation

---

## Contributing

When contributing to this project, please:
1. Follow the existing code style and conventions
2. Add appropriate tests for new functionality
3. Update documentation for API changes
4. Ensure thread safety is maintained
5. Validate on multiple ESP32 platforms if possible

## License

This project is licensed under the LGPL v3 License - see the LICENSE file for details.

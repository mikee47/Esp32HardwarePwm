## LedControl Library
## ESP32 LEDC PWM control library for Sming
##
## This component provides low-level access to the ESP32 LEDC peripheral
## with minimal overhead and maximum performance.

COMPONENT_LIBNAME := LedControl

# Source directories
COMPONENT_SRCDIRS := src
COMPONENT_INCDIRS := src/include

# Target platforms (ESP32 family only)
COMPONENT_SOC := esp32*

# Dependencies
COMPONENT_DEPENDS := \
	driver \
	soc

# Build flags
COMPONENT_CXXFLAGS := \
	-std=c++17 \
	-Wall \
	-Wextra \
	-O2

# ESP-IDF component requirements
COMPONENT_REQUIRES := driver soc

# Documentation
COMPONENT_DOCDIR := docs

$(COMPONENT_RULE)

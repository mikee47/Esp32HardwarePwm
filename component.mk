# ESP32 Hardware PWM Component for Sming Framework

COMPONENT_SRCDIRS := src
COMPONENT_INCDIRS := src/include

# ESP32 specific component - currently keeps esp8266 from building -
# COMPONENT_SOC := esp32*

# Required for C++ features
COMPONENT_CXXFLAGS += -std=c++17

# Add any specific compiler flags
ifdef ENABLE_DEBUG
    COMPONENT_CXXFLAGS += -DDEBUG_ESP_PWM
endif

# This creates the component library
#.PHONY: build
#build: $(COMPONENT_LIBDIR)/lib$(COMPONENT_NAME).a


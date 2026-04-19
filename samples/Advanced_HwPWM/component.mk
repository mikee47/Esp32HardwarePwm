DISABLE_NETWORK := 1

COMPONENT_DEPENDS := Esp32HardwarePwm

# Enable ISR→callback latency measurement (prints per-fade stats to Serial)
COMPONENT_CXXFLAGS += -DHW_PWM_MEASURE_LATENCY


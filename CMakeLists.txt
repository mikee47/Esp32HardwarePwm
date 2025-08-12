# ESP32 Hardware PWM Component for Sming Framework
# CMake build configuration for ESP-IDF v4.0+

idf_component_register(
    SRCS "src/Esp32HardwarePwm.cpp"
    INCLUDE_DIRS "src/include"
    REQUIRES driver esp_common
    PRIV_REQUIRES ""
)

# Set C++ standard
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# Component-specific compile definitions
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ESP32_HARDWARE_PWM_VERSION_MAJOR=1
    ESP32_HARDWARE_PWM_VERSION_MINOR=0
    ESP32_HARDWARE_PWM_VERSION_PATCH=0
)

# Optimization flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -ffunction-sections
    -fdata-sections
)

# Debug flags for debug builds
if(CONFIG_COMPILER_OPTIMIZATION_DEBUG)
    target_compile_options(${COMPONENT_LIB} PRIVATE -ggdb)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DEBUG_ESP_PWM=1)
endif()

# Thread safety support
target_compile_definitions(${COMPONENT_LIB} PRIVATE SUPPORT_LEDC_THREAD_SAFE=1)

# Component information
set_property(TARGET ${COMPONENT_LIB} PROPERTY DESCRIPTION "ESP32 LEDC PWM wrapper with resource management")
set_property(TARGET ${COMPONENT_LIB} PROPERTY VERSION "1.0.0")

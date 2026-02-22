#pragma once

#include <Arduino.h>

// Platform detection
#if defined(ESP32)
  #define PLATFORM_IS_ESP32 1
#else
  #define PLATFORM_IS_ESP32 0
#endif

#if defined(ESP8266)
  #define PLATFORM_IS_ESP8266 1
#else
  #define PLATFORM_IS_ESP8266 0
#endif

#if defined(ARDUINO_AVR_MEGA2560)
  #define PLATFORM_IS_MEGA 1
#else
  #define PLATFORM_IS_MEGA 0
#endif

#if defined(ARDUINO_AVR_UNO)
  #define PLATFORM_IS_UNO 1
#else
  #define PLATFORM_IS_UNO 0
#endif

#if defined(ARDUINO_AVR_NANO)
  #define PLATFORM_IS_NANO 1
#else
  #define PLATFORM_IS_NANO 0
#endif

#if PLATFORM_IS_ESP32
  #define DEVICE_TIER 3
#elif PLATFORM_IS_ESP8266
  #define DEVICE_TIER 2
#elif (PLATFORM_IS_MEGA || PLATFORM_IS_UNO || PLATFORM_IS_NANO)
  #define DEVICE_TIER 1
#else
  #error "Unsupported platform for INSANE firmware"
#endif

// Feature flags
#if (DEVICE_TIER >= 2)
  #define HAS_OTA 1
  #define HAS_ASYNC_MQTT 1
#else
  #define HAS_OTA 0
  #define HAS_ASYNC_MQTT 0
#endif

#if PLATFORM_IS_ESP32
  #define HAS_WATCHDOG 1
#else
  #define HAS_WATCHDOG 0
#endif

#if (PLATFORM_IS_UNO || PLATFORM_IS_NANO)
  #define MEMORY_TINY 1
#else
  #define MEMORY_TINY 0
#endif

#if MEMORY_TINY
  #define MQTT_BUFFER_SIZE 96
  #define JSON_BUFFER_SIZE 64
#else
  #define MQTT_BUFFER_SIZE 256
  #define JSON_BUFFER_SIZE 128
#endif

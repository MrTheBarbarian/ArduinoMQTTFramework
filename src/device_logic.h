#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "mqtt_layer.h"

namespace device_logic {

static const char TOPIC_COUNTER[] = "insane/device/counter";
static const char TOPIC_LED_STATE[] = "insane/device/led/state";
static const char TOPIC_LED_SET[] = "insane/device/led/set";

static const uint32_t PUBLISH_INTERVAL_MS = 10000UL;

static uint32_t g_counter = 0;
static bool g_ledState = false;
static uint32_t g_lastPublishMs = 0;
static uint8_t g_ledPin = LED_BUILTIN;

inline void apply_led_state(bool on) {
  g_ledState = on;
  digitalWrite(g_ledPin, on ? HIGH : LOW);
  mqtt_publish(TOPIC_LED_STATE, on ? "ON" : "OFF", true);
}

inline void on_mqtt_message(const char* topic, const char* payload) {
  if (topic == nullptr || payload == nullptr) {
    return;
  }

  if (strcmp(topic, TOPIC_LED_SET) == 0) {
    if (strcmp(payload, "ON") == 0) {
      apply_led_state(true);
    } else if (strcmp(payload, "OFF") == 0) {
      apply_led_state(false);
    }
  }
}

inline void on_mqtt_connected() {
  mqtt_subscribe(TOPIC_LED_SET);
  mqtt_publish(TOPIC_LED_STATE, g_ledState ? "ON" : "OFF", true);
}

inline void init(uint8_t ledPin = LED_BUILTIN) {
  g_ledPin = ledPin;
  pinMode(g_ledPin, OUTPUT);
  apply_led_state(false);
  g_lastPublishMs = millis();
  mqtt_set_message_callback(on_mqtt_message);
}

inline void loop() {
  const uint32_t now = millis();
  if ((uint32_t)(now - g_lastPublishMs) >= PUBLISH_INTERVAL_MS) {
    g_lastPublishMs = now;
    g_counter++;

    char counterPayload[16];
    ultoa(g_counter, counterPayload, 10);

    mqtt_publish(TOPIC_COUNTER, counterPayload, true);
    mqtt_publish(TOPIC_LED_STATE, g_ledState ? "ON" : "OFF", true);
  }
}

}  // namespace device_logic

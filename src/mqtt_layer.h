#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "platform_capabilities.h"

#if HAS_ASYNC_MQTT
  #if PLATFORM_IS_ESP32
    #include <WiFi.h>
    #include <AsyncTCP.h>
  #elif PLATFORM_IS_ESP8266
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
  #endif
  #include <AsyncMqttClient.h>
#else
  #include <SPI.h>
  #include <Ethernet.h>
  #include <PubSubClient.h>
#endif

typedef void (*MqttMessageCallback)(const char* topic, const char* payload);

inline void mqtt_set_message_callback(MqttMessageCallback cb);
inline void mqtt_init(const char* host, uint16_t port, const char* clientId);
inline void mqtt_loop();
inline bool mqtt_publish(const char* topic, const char* payload, bool retained);
inline bool mqtt_subscribe(const char* topic);
inline bool mqtt_is_connected();

namespace mqtt_layer_internal {

static MqttMessageCallback g_messageCb = nullptr;

#if HAS_ASYNC_MQTT
static AsyncMqttClient g_mqtt;
static uint32_t g_retryBackoffMs = 1000;
static const uint32_t RETRY_BACKOFF_MAX_MS = 60000;
static uint32_t g_nextRetryAtMs = 0;

static char g_rxPayload[MQTT_BUFFER_SIZE];

inline void schedule_reconnect() {
  const uint32_t now = millis();
  g_nextRetryAtMs = now + g_retryBackoffMs;
  if (g_retryBackoffMs < RETRY_BACKOFF_MAX_MS) {
    g_retryBackoffMs <<= 1;
    if (g_retryBackoffMs > RETRY_BACKOFF_MAX_MS) {
      g_retryBackoffMs = RETRY_BACKOFF_MAX_MS;
    }
  }
}

inline void on_mqtt_connect(bool) {
  g_retryBackoffMs = 1000;
}

inline void on_mqtt_disconnect(AsyncMqttClientDisconnectReason) {
  schedule_reconnect();
}

inline void on_mqtt_message(
  char* topic,
  char* payload,
  AsyncMqttClientMessageProperties,
  size_t len,
  size_t index,
  size_t total
) {
  if (g_messageCb == nullptr || topic == nullptr || payload == nullptr) {
    return;
  }

  if (index != 0 || len != total) {
    // Small-RAM safe path: process only single-frame payloads.
    return;
  }

  size_t copyLen = len;
  if (copyLen >= sizeof(g_rxPayload)) {
    copyLen = sizeof(g_rxPayload) - 1;
  }

  memcpy(g_rxPayload, payload, copyLen);
  g_rxPayload[copyLen] = '\0';
  g_messageCb(topic, g_rxPayload);
}

#else
static EthernetClient g_ethClient;
static PubSubClient g_mqtt(g_ethClient);

static const uint32_t RETRY_INTERVAL_MS = 5000;
static uint32_t g_lastRetryMs = 0;

static char g_host[40] = {0};
static uint16_t g_port = 1883;
static char g_clientId[24] = {0};

inline void on_mqtt_message(char* topic, byte* payload, unsigned int len) {
  if (g_messageCb == nullptr || topic == nullptr || payload == nullptr) {
    return;
  }

  char payloadBuffer[MQTT_BUFFER_SIZE];
  unsigned int copyLen = len;
  if (copyLen >= sizeof(payloadBuffer)) {
    copyLen = sizeof(payloadBuffer) - 1;
  }

  memcpy(payloadBuffer, payload, copyLen);
  payloadBuffer[copyLen] = '\0';
  g_messageCb(topic, payloadBuffer);
}

inline bool connect_if_needed() {
  if (g_mqtt.connected()) {
    return true;
  }

  const uint32_t now = millis();
  if ((uint32_t)(now - g_lastRetryMs) < RETRY_INTERVAL_MS) {
    return false;
  }
  g_lastRetryMs = now;

  return g_mqtt.connect(
    g_clientId,
    "insane/device/status",
    1,
    true,
    "offline"
  );
}
#endif

}  // namespace mqtt_layer_internal

inline void mqtt_set_message_callback(MqttMessageCallback cb) {
  mqtt_layer_internal::g_messageCb = cb;
}

inline void mqtt_init(const char* host, uint16_t port, const char* clientId) {
#if HAS_ASYNC_MQTT
  mqtt_layer_internal::g_mqtt.onConnect(mqtt_layer_internal::on_mqtt_connect);
  mqtt_layer_internal::g_mqtt.onDisconnect(mqtt_layer_internal::on_mqtt_disconnect);
  mqtt_layer_internal::g_mqtt.onMessage(mqtt_layer_internal::on_mqtt_message);

  mqtt_layer_internal::g_mqtt.setServer(host, port);
  mqtt_layer_internal::g_mqtt.setClientId(clientId);
  mqtt_layer_internal::g_mqtt.setWill("insane/device/status", 1, true, "offline");
  mqtt_layer_internal::g_mqtt.connect();
#else
  strncpy(mqtt_layer_internal::g_host, host, sizeof(mqtt_layer_internal::g_host) - 1);
  mqtt_layer_internal::g_port = port;
  strncpy(mqtt_layer_internal::g_clientId, clientId, sizeof(mqtt_layer_internal::g_clientId) - 1);

  mqtt_layer_internal::g_mqtt.setServer(mqtt_layer_internal::g_host, mqtt_layer_internal::g_port);
  mqtt_layer_internal::g_mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt_layer_internal::g_mqtt.setCallback(mqtt_layer_internal::on_mqtt_message);
  (void)mqtt_layer_internal::connect_if_needed();
#endif
}

inline void mqtt_loop() {
#if HAS_ASYNC_MQTT
  if (!mqtt_layer_internal::g_mqtt.connected()) {
    const uint32_t now = millis();
    if ((int32_t)(now - mqtt_layer_internal::g_nextRetryAtMs) >= 0) {
      mqtt_layer_internal::g_mqtt.connect();
      mqtt_layer_internal::schedule_reconnect();
    }
  }
#else
  (void)mqtt_layer_internal::connect_if_needed();
  mqtt_layer_internal::g_mqtt.loop();
#endif
}

inline bool mqtt_publish(const char* topic, const char* payload, bool retained) {
  if (topic == nullptr || payload == nullptr) {
    return false;
  }

#if HAS_ASYNC_MQTT
  if (!mqtt_layer_internal::g_mqtt.connected()) {
    return false;
  }
  const uint16_t packetId = mqtt_layer_internal::g_mqtt.publish(topic, 1, retained, payload);
  return packetId > 0;
#else
  if (!mqtt_layer_internal::g_mqtt.connected()) {
    return false;
  }
  return mqtt_layer_internal::g_mqtt.publish(topic, payload, retained);
#endif
}

inline bool mqtt_subscribe(const char* topic) {
  if (topic == nullptr) {
    return false;
  }

#if HAS_ASYNC_MQTT
  if (!mqtt_layer_internal::g_mqtt.connected()) {
    return false;
  }
  const uint16_t packetId = mqtt_layer_internal::g_mqtt.subscribe(topic, 1);
  return packetId > 0;
#else
  if (!mqtt_layer_internal::g_mqtt.connected()) {
    return false;
  }
  return mqtt_layer_internal::g_mqtt.subscribe(topic);
#endif
}

inline bool mqtt_is_connected() {
  return mqtt_layer_internal::g_mqtt.connected();
}

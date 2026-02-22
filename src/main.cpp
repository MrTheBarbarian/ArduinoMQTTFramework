#include <Arduino.h>

#include "platform_capabilities.h"
#include "mqtt_layer.h"
#include "device_logic.h"

#if PLATFORM_IS_ESP32
  #include <WiFi.h>
#elif PLATFORM_IS_ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <SPI.h>
  #include <Ethernet.h>
#endif

#if HAS_OTA
  #include <ArduinoOTA.h>
#endif

#if HAS_WATCHDOG
  #include <esp_task_wdt.h>
#endif

namespace app_config {

static const char WIFI_SSID[] = "YOUR_WIFI_SSID";
static const char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";

static const char MQTT_HOST[] = "192.168.1.100";
static const uint16_t MQTT_PORT = 1883;
static const char MQTT_CLIENT_ID[] = "insane-device";

#if !HAS_ASYNC_MQTT
static byte ETH_MAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01};
#endif

}  // namespace app_config

static bool g_prevMqttConnected = false;

static void network_init() {
#if HAS_ASYNC_MQTT
  WiFi.mode(WIFI_STA);
  WiFi.begin(app_config::WIFI_SSID, app_config::WIFI_PASS);
#else
  Ethernet.begin(app_config::ETH_MAC);
#endif
}

#if HAS_OTA
static void ota_init() {
  ArduinoOTA.setHostname(app_config::MQTT_CLIENT_ID);
  ArduinoOTA.begin();
}
#endif

static void on_mqtt_connected_edge() {
  mqtt_publish("insane/device/status", "online", true);
  device_logic::on_mqtt_connected();
}

void setup() {
  Serial.begin(115200);

#if HAS_WATCHDOG
  esp_task_wdt_init(8, true);
  esp_task_wdt_add(nullptr);
#endif

  network_init();
  device_logic::init(LED_BUILTIN);
  mqtt_init(app_config::MQTT_HOST, app_config::MQTT_PORT, app_config::MQTT_CLIENT_ID);

#if HAS_OTA
  ota_init();
#endif
}

void loop() {
#if HAS_OTA
  ArduinoOTA.handle();
#endif

#if HAS_WATCHDOG
  esp_task_wdt_reset();
#endif

  mqtt_loop();
  device_logic::loop();

  const bool mqttConnected = mqtt_is_connected();
  if (mqttConnected && !g_prevMqttConnected) {
    on_mqtt_connected_edge();
  }
  g_prevMqttConnected = mqttConnected;
}

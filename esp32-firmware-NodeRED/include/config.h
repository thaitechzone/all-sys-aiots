#pragma once

// ============================================================
//  config.h — แก้ไขค่าตรงนี้ก่อน Flash ทุกครั้ง
// ============================================================

// ────────────────────────────────────────────────────────────
//  WiFi Credentials
// ────────────────────────────────────────────────────────────
#define WIFI_SSID       "Casa_Thalar"
#define WIFI_PASSWORD   "casa1234"

// ────────────────────────────────────────────────────────────
//  Device Identity
// ────────────────────────────────────────────────────────────
#define DEVICE_ID       "ESP32-FARM-001"
#define LOCATION        "farm"

// ────────────────────────────────────────────────────────────
//  MQTT Broker
// ────────────────────────────────────────────────────────────
#define MQTT_HOST       "192.168.1.121"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASS       ""

// ────────────────────────────────────────────────────────────
//  MQTT Topics
// ────────────────────────────────────────────────────────────
#define TOPIC_TELEMETRY "smartfarm/" DEVICE_ID "/telemetry"
#define TOPIC_STATUS    "smartfarm/" DEVICE_ID "/status"
#define TOPIC_COMMAND   "smartfarm/" DEVICE_ID "/command"

// ────────────────────────────────────────────────────────────
//  Sensor Pins
//  DS18B20 — OneWire temperature sensor
//  ถ้าไม่ต่อ sensor จะสุ่มค่าอัตโนมัติ
// ────────────────────────────────────────────────────────────
#define DS18B20_PIN     14    // GPIO14 — DS18B20 Data (OneWire)
#define RELAY_PIN       26    // GPIO26 — Relay Control Output

// ────────────────────────────────────────────────────────────
//  Timing
// ────────────────────────────────────────────────────────────
#define PUBLISH_INTERVAL_MS   5000
#define STATUS_INTERVAL_MS    30000
#define WIFI_TIMEOUT_MS       15000
#define MQTT_RECONNECT_MS     5000

// ────────────────────────────────────────────────────────────
//  Built-in LED
// ────────────────────────────────────────────────────────────
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// ────────────────────────────────────────────────────────────
//  Serial Debug
// ────────────────────────────────────────────────────────────
#define SERIAL_BAUD     115200
#define DEBUG_ENABLED   1

#if DEBUG_ENABLED
  #define DBG(x)    Serial.print(x)
  #define DBGLN(x)  Serial.println(x)
  #define DBGF(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG(x)
  #define DBGLN(x)
  #define DBGF(...)
#endif

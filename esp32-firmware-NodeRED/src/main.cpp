// ============================================================
//  ESP32 SmartFarm — MQTT Telemetry
//
//  Temperature : DS18B20 บน GPIO14 (OneWire)
//               ถ้าไม่ต่อ sensor (คืน -127) → สุ่มค่าอัตโนมัติ
//  Humidity / Soil : simulated drift
//
//  Topics:
//    PUB  smartfarm/<DEVICE_ID>/telemetry  → sensor data (JSON)
//    PUB  smartfarm/<DEVICE_ID>/status     → online/offline
//    SUB  smartfarm/<DEVICE_ID>/command    → relay control
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// ────────────────────────────────────────────────────────────
//  DS18B20
// ────────────────────────────────────────────────────────────
OneWire           oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
bool              sensorFound = false;

// ────────────────────────────────────────────────────────────
//  Global Objects
// ────────────────────────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// ────────────────────────────────────────────────────────────
//  Simulated state (ใช้เมื่อไม่ต่อ sensor หรือ sensor error)
// ────────────────────────────────────────────────────────────
float simTemperature  = 28.0f;
float simHumidity     = 65.0f;
float simSoilMoisture = 45.0f;

bool  relayState          = false;
unsigned long lastPublishTime   = 0;
unsigned long lastStatusTime    = 0;
unsigned long lastReconnectTime = 0;

// ────────────────────────────────────────────────────────────
//  Forward Declarations
// ────────────────────────────────────────────────────────────
void  connectWiFi();
bool  connectMQTT();
void  publishTelemetry();
void  publishStatus(const char* state);
void  mqttCallback(char* topic, byte* payload, unsigned int length);
float readTemperature();
void  setRelay(bool state);
void  updateSimValues();
void  printBanner();

// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pinMode(LED_BUILTIN, OUTPUT);

    // DS18B20 init — ตรวจว่าต่อ sensor จริงหรือเปล่า
    ds18b20.begin();
    int deviceCount = ds18b20.getDeviceCount();
    sensorFound = (deviceCount > 0);

    printBanner();

    connectWiFi();

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60);
    mqttClient.setBufferSize(512);

    connectMQTT();
}

// ============================================================
//  loop()
// ============================================================
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        DBGLN("[WiFi] Disconnected — reconnecting...");
        connectWiFi();
    }

    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectTime >= MQTT_RECONNECT_MS) {
            lastReconnectTime = now;
            connectMQTT();
        }
    }

    mqttClient.loop();

    unsigned long now = millis();

    if (now - lastPublishTime >= PUBLISH_INTERVAL_MS) {
        lastPublishTime = now;
        updateSimValues();  // อัพเดต drift ก่อนส่ง
        publishTelemetry();
    }

    if (now - lastStatusTime >= STATUS_INTERVAL_MS) {
        lastStatusTime = now;
        publishStatus("online");
    }
}

// ============================================================
//  อัพเดตค่า simulated (drift ทีละน้อย)
// ============================================================
void updateSimValues() {
    simTemperature  += (random(-5, 6) * 0.1f);
    simHumidity     += (random(-3, 4) * 0.1f);
    simSoilMoisture += (random(-2, 3) * 0.5f);

    simTemperature  = constrain(simTemperature,  18.0f, 40.0f);
    simHumidity     = constrain(simHumidity,     30.0f, 95.0f);
    simSoilMoisture = constrain(simSoilMoisture, 10.0f, 90.0f);
}

// ============================================================
//  WiFi
// ============================================================
void connectWiFi() {
    DBGF("\n[WiFi] Connecting to: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            DBGLN("[WiFi] Timeout! Restarting...");
            ESP.restart();
        }
        delay(500);
        DBG(".");
    }
    DBGLN();
    DBGF("[WiFi] IP: %s  RSSI: %d dBm\n",
         WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// ============================================================
//  MQTT
// ============================================================
bool connectMQTT() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String clientId = String(DEVICE_ID) + "-" + mac.substring(8);

    DBGF("[MQTT] Connecting to %s:%d as %s\n",
         MQTT_HOST, MQTT_PORT, clientId.c_str());

    String willTopic = String(TOPIC_STATUS);
    String willMsg   = "{\"device\":\"" DEVICE_ID "\",\"state\":\"offline\"}";

    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(
            clientId.c_str(), MQTT_USER, MQTT_PASS,
            willTopic.c_str(), 1, true, willMsg.c_str());
    } else {
        connected = mqttClient.connect(
            clientId.c_str(), nullptr, nullptr,
            willTopic.c_str(), 1, true, willMsg.c_str());
    }

    if (connected) {
        DBGLN("[MQTT] Connected!");
        mqttClient.subscribe(TOPIC_COMMAND, 1);
        publishStatus("online");
        return true;
    }
    DBGF("[MQTT] Failed rc=%d, retry in %d ms\n",
         mqttClient.state(), MQTT_RECONNECT_MS);
    return false;
}

// ============================================================
//  Publish Telemetry
// ============================================================
void publishTelemetry() {
    if (!mqttClient.connected()) return;

    float temp = readTemperature();
    int   rssi = WiFi.RSSI();

    JsonDocument doc;
    doc["device"]        = DEVICE_ID;
    doc["location"]      = LOCATION;
    doc["temperature"]   = round(temp * 10.0f) / 10.0f;
    doc["humidity"]      = round(simHumidity * 10.0f) / 10.0f;
    doc["soil_moisture"] = (int)simSoilMoisture;
    doc["relay"]         = relayState ? 1 : 0;
    doc["rssi"]          = rssi;
    doc["sensor"]        = sensorFound ? "ds18b20" : "simulated";

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf));

    bool ok = mqttClient.publish(TOPIC_TELEMETRY, buf, false);
    if (ok) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
        DBGF("[PUB] %s (%d bytes)\n", buf, (int)len);
    } else {
        DBGLN("[PUB] FAILED");
    }
}

// ============================================================
//  Publish Status
// ============================================================
void publishStatus(const char* state) {
    if (!mqttClient.connected()) return;

    JsonDocument doc;
    doc["device"]   = DEVICE_ID;
    doc["state"]    = state;
    doc["ip"]       = WiFi.localIP().toString();
    doc["rssi"]     = WiFi.RSSI();
    doc["uptime_s"] = millis() / 1000;
    doc["sensor"]   = sensorFound ? "ds18b20" : "simulated";

    char buf[200];
    serializeJson(doc, buf, sizeof(buf));
    mqttClient.publish(TOPIC_STATUS, buf, true);
    DBGF("[PUB] Status: %s\n", buf);
}

// ============================================================
//  MQTT Callback
// ============================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char msg[256];
    length = min(length, (unsigned int)(sizeof(msg) - 1));
    memcpy(msg, payload, length);
    msg[length] = '\0';

    DBGF("[SUB] %s | %s\n", topic, msg);

    JsonDocument doc;
    if (deserializeJson(doc, msg)) return;

    if (doc["relay"].is<int>()) {
        setRelay(doc["relay"].as<int>() == 1);
    } else if (doc["relay"].is<const char*>()) {
        String r = doc["relay"].as<String>();
        r.toLowerCase();
        setRelay(r == "on" || r == "true" || r == "1");
    }

    if (doc["restart"].as<bool>()) {
        DBGLN("[CMD] Restart!");
        publishStatus("restarting");
        delay(500);
        ESP.restart();
    }
}

// ============================================================
//  Read Temperature — DS18B20 with auto-fallback to simulated
// ============================================================
float readTemperature() {
    if (!sensorFound) return simTemperature;

    ds18b20.requestTemperatures();
    float t = ds18b20.getTempCByIndex(0);

    // DEVICE_DISCONNECTED_C = -127.0 หมายถึงไม่มี sensor
    if (t == DEVICE_DISCONNECTED_C || t < -55.0f || t > 125.0f) {
        sensorFound = false;
        DBGLN("[Sensor] DS18B20 disconnected — switching to simulated");
        return simTemperature;
    }
    return t;
}

// ============================================================
//  Relay Control
// ============================================================
void setRelay(bool state) {
    relayState = state;
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
    DBGF("[RELAY] %s\n", state ? "ON" : "OFF");

    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["relay"]  = state ? 1 : 0;
    doc["source"] = "command";

    char buf[128];
    serializeJson(doc, buf, sizeof(buf));
    mqttClient.publish(TOPIC_TELEMETRY, buf, false);
}

// ============================================================
//  Banner
// ============================================================
void printBanner() {
    DBGLN("\n================================================");
    DBGLN("  ESP32 SmartFarm — MQTT Telemetry");
    DBGLN("================================================");
    DBGF("  Device   : %s\n",   DEVICE_ID);
    DBGF("  Broker   : %s:%d\n", MQTT_HOST, MQTT_PORT);
    DBGF("  DS18B20  : GPIO%d\n", DS18B20_PIN);
    if (sensorFound) {
        DBGLN("  Sensor   : DS18B20 CONNECTED");
    } else {
        DBGLN("  Sensor   : NOT FOUND — using simulated values");
    }
    DBGLN("================================================\n");
}

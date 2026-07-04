# ESP32 SmartFarm — MQTT Telemetry Firmware

> 🏠 เป็นส่วนหนึ่งของโปรเจกต์ [IoT Stack](../README.md) — firmware ฝั่งอุปกรณ์ที่ส่งข้อมูลเข้า Mosquitto → Node-RED → InfluxDB
>
> PlatformIO project สำหรับส่งข้อมูล sensor ไปยัง MQTT Broker ทุก 5 วินาที
>
> **Temperature sensor:** DS18B20 บน GPIO14 (OneWire)
> ถ้าไม่ต่อ sensor — สุ่มค่าอัตโนมัติ (ไม่ต้อง config อะไร)

---

## โครงสร้างไฟล์

```
esp32-firmware-NodeRED/
  ├── platformio.ini          ← Project config + dependencies
  ├── include/
  │     └── config.h          ← ★ แก้ WiFi / MQTT / Pins ที่นี่
  ├── src/
  │     └── main.cpp          ← Main firmware
  └── test/
        └── test_payload.py   ← Python simulator (ไม่ต้องใช้ ESP32)
```

---

## ขั้นตอนที่ 1 — ติดตั้ง VS Code + PlatformIO

1. ติดตั้ง [VS Code](https://code.visualstudio.com/)
2. เปิด Extensions (`Ctrl+Shift+X`) → ค้นหา **PlatformIO IDE** → Install
3. รอ PlatformIO ติดตั้ง toolchain (ครั้งแรกใช้เวลา 5–10 นาที)

---

## ขั้นตอนที่ 2 — เปิด Project

```
File → Open Folder → เลือกโฟลเดอร์ esp32-firmware-NodeRED (ในโปรเจกต์นี้)
```

PlatformIO จะตรวจพบ `platformio.ini` อัตโนมัติและดาวน์โหลด libraries

---

## ขั้นตอนที่ 3 — แก้ไข config.h

เปิดไฟล์ `include/config.h` แล้วแก้:

```cpp
// WiFi
#define WIFI_SSID       "ชื่อ WiFi ของคุณ"
#define WIFI_PASSWORD   "รหัส WiFi ของคุณ"

// MQTT Broker — เลือก 1 ตัว
// ตัวเลือก A: Local Mosquitto บน Windows (แนะนำ)
#define MQTT_HOST   "192.168.x.x"  // IP เครื่อง Windows ที่รัน Docker

// ตัวเลือก B: HiveMQ Public (ทดสอบโดยไม่ต้องมี server เอง)
// #define MQTT_HOST   "broker.hivemq.com"
```

### หา IP Windows สำหรับ Mosquitto Local

```powershell
ipconfig | findstr "IPv4"
```

---

## ขั้นตอนที่ 4 — Build & Upload

```
# Build เพื่อตรวจ error
Ctrl+Alt+B   หรือ คลิก ✓ ที่ Status Bar ด้านล่าง

# Upload ไปยัง ESP32 (ต่อ USB ก่อน)
Ctrl+Alt+U   หรือ คลิก → ที่ Status Bar

# เปิด Serial Monitor ดู log
Ctrl+Alt+S   หรือ คลิก 🔌 ที่ Status Bar
```

---

## Pin Map

| GPIO | บทบาท | หมายเหตุ |
|------|-------|---------|
| 14 | DS18B20 Data (OneWire) | ต้องใช้ pull-up 4.7kΩ ไป 3.3V |
| 26 | Relay Output | Active HIGH |
| 2  | LED_BUILTIN | กระพริบเมื่อส่งข้อมูล |

### วงจร DS18B20

```
DS18B20 VCC  → 3.3V
DS18B20 GND  → GND
DS18B20 DATA → GPIO14 + ตัวต้านทาน 4.7kΩ ระหว่าง DATA และ 3.3V
```

---

## Sensor Auto-Detection

Firmware ตรวจสอบ DS18B20 ตอน boot อัตโนมัติ:

| สถานะ | ผล |
|-------|-----|
| พบ DS18B20 | อ่านค่าอุณหภูมิจริงจาก sensor |
| ไม่พบ DS18B20 | สุ่มค่า temperature แบบ random walk (18–40°C) |
| Disconnect ระหว่างรัน | สลับเป็น simulated อัตโนมัติ |

Humidity และ Soil Moisture เป็น simulated เสมอ (DS18B20 วัดได้เฉพาะ temperature)

---

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `smartfarm/ESP32-FARM-001/telemetry` | PUB (ทุก 5s) | JSON sensor data |
| `smartfarm/ESP32-FARM-001/status` | PUB (ทุก 30s + LWT) | `{"state":"online/offline"}` |
| `smartfarm/ESP32-FARM-001/command` | SUB | `{"relay":1}` หรือ `{"relay":"on"}` |

### ตัวอย่าง Payload — Telemetry

```json
{
    "device": "ESP32-FARM-001",
    "location": "farm",
    "temperature": 28.5,
    "humidity": 65.2,
    "soil_moisture": 42,
    "relay": 0,
    "rssi": -68,
    "sensor": "ds18b20"
}
```

> field `"sensor"` จะเป็น `"ds18b20"` หรือ `"simulated"` — บอกสถานะว่าค่า temperature มาจากที่ไหน

### ตัวอย่าง Command — ควบคุม Relay

```bash
# เปิด relay (ผ่าน Local Mosquitto)
docker exec mosquitto mosquitto_pub \
    -t "smartfarm/ESP32-FARM-001/command" \
    -m '{"relay":1}'

# ปิด relay
docker exec mosquitto mosquitto_pub \
    -t "smartfarm/ESP32-FARM-001/command" \
    -m '{"relay":0}'

# Restart ESP32
docker exec mosquitto mosquitto_pub \
    -t "smartfarm/ESP32-FARM-001/command" \
    -m '{"restart":true}'
```

---

## ทดสอบโดยไม่ต้องใช้ ESP32 (Python Simulator)

```bash
# ติดตั้ง dependency
pip install paho-mqtt

# ส่งข้อมูลไปยัง Local Mosquitto
python test/test_payload.py --broker 192.168.1.100

# ส่งไปยัง HiveMQ Public
python test/test_payload.py

# จำลองหลาย device
python test/test_payload.py --device ESP32-FARM-002

# จำกัดจำนวน messages
python test/test_payload.py --count 30 --interval 2
```

---

## Serial Output ตัวอย่าง

**กรณีต่อ DS18B20:**
```
================================================
  ESP32 SmartFarm — MQTT Telemetry
================================================
  Device   : ESP32-FARM-001
  Broker   : 192.168.1.121:1883
  DS18B20  : GPIO14
  Sensor   : DS18B20 CONNECTED
================================================

[WiFi] IP: 192.168.1.105  RSSI: -52 dBm
[MQTT] Connected!
[PUB] {"device":"ESP32-FARM-001","temperature":28.5,"sensor":"ds18b20",...}
```

**กรณีไม่ต่อ sensor:**
```
  Sensor   : NOT FOUND — using simulated values

[PUB] {"device":"ESP32-FARM-001","temperature":30.1,"sensor":"simulated",...}
```

---

## Troubleshooting

| อาการ | วิธีแก้ |
|-------|---------|
| Upload ไม่ได้ — `No device found` | กด **BOOT button** ค้างไว้ขณะ upload แล้วปล่อย |
| WiFi ไม่ต่อ — restart loop | ตรวจ SSID/Password ใน config.h |
| MQTT ไม่ต่อ rc=-2 | ตรวจ MQTT_HOST — ใส่ IP จริงของ host ไม่ใช่ localhost |
| ข้อมูลไม่ถึง Node-RED | ตรวจ topic ให้ตรงกัน ทั้ง ESP32 และ MQTT In node |
| temperature = -127 | DS18B20 ไม่ต่อหรือ wiring ผิด — firmware จะสลับเป็น simulated เอง |
| temperature ผิดปกติ | ตรวจ pull-up resistor 4.7kΩ ระหว่าง DATA และ 3.3V |

"""
test_payload.py — ทดสอบ MQTT Payload จาก PC โดยไม่ต้องใช้ ESP32
ใช้ paho-mqtt เพื่อ simulate การส่ง telemetry

Requirements:
    pip install paho-mqtt

Usage:
    python test/test_payload.py
    python test/test_payload.py --broker 192.168.1.100
    python test/test_payload.py --device ESP32-FARM-002 --count 20
"""

import json
import time
import math
import random
import argparse
import paho.mqtt.client as mqtt

# ── Config ────────────────────────────────────────────────
DEFAULT_BROKER  = "broker.hivemq.com"
DEFAULT_PORT    = 1883
DEFAULT_DEVICE  = "ESP32-FARM-001"
DEFAULT_COUNT   = 0          # 0 = infinite
DEFAULT_INTERVAL = 5.0       # seconds

# ── Argument Parser ───────────────────────────────────────
parser = argparse.ArgumentParser(description="ESP32 MQTT Telemetry Simulator")
parser.add_argument("--broker",   default=DEFAULT_BROKER,   help="MQTT broker hostname")
parser.add_argument("--port",     default=DEFAULT_PORT,     type=int, help="MQTT port")
parser.add_argument("--device",   default=DEFAULT_DEVICE,   help="Device ID")
parser.add_argument("--count",    default=DEFAULT_COUNT,    type=int, help="Number of messages (0=infinite)")
parser.add_argument("--interval", default=DEFAULT_INTERVAL, type=float, help="Interval in seconds")
args = parser.parse_args()

TOPIC_TELEMETRY = f"smartfarm/{args.device}/telemetry"
TOPIC_STATUS    = f"smartfarm/{args.device}/status"


# ── MQTT Callbacks ────────────────────────────────────────
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"[MQTT] Connected to {args.broker}:{args.port}")
        print(f"[MQTT] Publishing to: {TOPIC_TELEMETRY}")
        print(f"[MQTT] Press Ctrl+C to stop\n")
    else:
        print(f"[MQTT] Connection failed: rc={rc}")


def on_publish(client, userdata, mid, reason_code=None, properties=None):
    pass  # silent ok


def on_disconnect(client, userdata, disconnect_flags, reason_code=None, properties=None):
    print(f"[MQTT] Disconnected: rc={reason_code}")


# ── Sensor Simulation ─────────────────────────────────────
class SensorSimulator:
    def __init__(self):
        self.t = 28.0
        self.h = 65.0
        self.s = 45.0

    def next(self):
        """ขยับค่า sensor ทีละน้อย (random walk)"""
        self.t += random.uniform(-0.5, 0.5)
        self.h += random.uniform(-1.0, 1.0)
        self.s += random.uniform(-2.0, 2.0)

        self.t = max(18.0, min(40.0, self.t))
        self.h = max(30.0, min(95.0, self.h))
        self.s = max(10.0, min(90.0, self.s))

        return {
            "device": args.device,
            "location": "farm",
            "temperature": round(self.t, 1),
            "humidity": round(self.h, 1),
            "soil_moisture": int(self.s),
            "relay": 0,
            "rssi": random.randint(-85, -55),
        }


# ── Main ──────────────────────────────────────────────────
def main():
    client = mqtt.Client(
        client_id=f"{args.device}-py-sim",
        protocol=mqtt.MQTTv5
    )
    client.on_connect    = on_connect
    client.on_publish    = on_publish
    client.on_disconnect = on_disconnect

    # Last Will
    will_payload = json.dumps({"device": args.device, "state": "offline"})
    client.will_set(TOPIC_STATUS, will_payload, qos=1, retain=True)

    print(f"[MQTT] Connecting to {args.broker}:{args.port}...")
    client.connect(args.broker, args.port, keepalive=60)
    client.loop_start()

    # Wait for connection
    time.sleep(1.5)

    # Publish online status
    status_payload = json.dumps({"device": args.device, "state": "online", "source": "python-sim"})
    client.publish(TOPIC_STATUS, status_payload, qos=1, retain=True)

    sim = SensorSimulator()
    count = 0

    try:
        while True:
            count += 1
            payload = sim.next()
            msg = json.dumps(payload)

            result = client.publish(TOPIC_TELEMETRY, msg, qos=1)
            result.wait_for_publish(timeout=3.0)

            print(f"[{count:04d}] {TOPIC_TELEMETRY}")
            print(f"       T={payload['temperature']}°C  "
                  f"H={payload['humidity']}%  "
                  f"Soil={payload['soil_moisture']}%  "
                  f"RSSI={payload['rssi']}dBm")

            if args.count > 0 and count >= args.count:
                print(f"\n[Done] Sent {count} messages")
                break

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print(f"\n[Stop] Sent {count} messages total")
    finally:
        offline = json.dumps({"device": args.device, "state": "offline"})
        client.publish(TOPIC_STATUS, offline, qos=1, retain=True)
        time.sleep(0.5)
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()

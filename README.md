# IoT Stack — Node-RED + N8N + Home Assistant + InfluxDB + Grafana

> ระบบ IoT / Automation แบบครบชุด รันด้วย Docker Compose ชุดเดียว
> พร้อม MQTT Broker, Reverse Proxy และ Cloudflare Tunnel สำหรับเปิดออก Internet

คู่มือนี้อธิบาย **ทีละขั้นตอน** ตั้งแต่เครื่องเปล่า จนระบบขึ้นและใช้งานได้จริง

---

## 📦 ระบบประกอบด้วยอะไรบ้าง

| Service | หน้าที่ | Local URL | Container port |
|---|---|---|---|
| **Mosquitto** | MQTT Broker (รับส่งข้อมูลจากอุปกรณ์ IoT / ESP32) | `mqtt://localhost:1883`<br>WebSocket `:9001` | 1883, 9001 |
| **InfluxDB** | Time-Series Database (เก็บค่าเซนเซอร์ตามเวลา) | http://localhost:8086 | 8086 |
| **Node-RED** | Flow automation (เชื่อม MQTT → InfluxDB, logic) | http://localhost:1880 | 1880 |
| **N8N** | Workflow automation (เชื่อม API / แจ้งเตือน) | http://localhost:5678 | 5678 |
| **Home Assistant** | ศูนย์รวม Home Automation | http://localhost:8123 | 8123 |
| **Grafana** | Dashboard / กราฟข้อมูล | http://localhost:3000 | 3000 |
| **Nginx Proxy Manager** | Reverse Proxy + SSL | http://localhost:81 (admin) | 80, 443, 81 |
| **Cloudflared** | Cloudflare Tunnel (เปิดออก Internet) | — | — |

ทุก service อยู่ใน Docker network เดียวกันชื่อ `iot_network` → เรียกหากันด้วย **ชื่อ container** ได้ เช่น Node-RED เชื่อม InfluxDB ผ่าน `http://influxdb:8086`

### แผนภาพการไหลของข้อมูล

```
  ESP32 / อุปกรณ์ IoT
        │  (MQTT publish)
        ▼
  ┌─────────────┐     ┌──────────┐     ┌──────────┐
  │  Mosquitto  │ ──▶ │ Node-RED │ ──▶ │ InfluxDB │
  │ (MQTT 1883) │     │  (logic) │     │(ข้อมูลเวลา)│
  └─────────────┘     └──────────┘     └────┬─────┘
                                            │
                                            ▼
                                       ┌──────────┐
                                       │ Grafana  │  (แสดงกราฟ)
                                       └──────────┘

  Internet ──▶ Cloudflare Tunnel ──▶ Nginx Proxy Manager ──▶ N8N / services
```

---

## ✅ สิ่งที่ต้องมีก่อนเริ่ม (Prerequisites)

1. **Docker Desktop** (Windows / Mac) หรือ Docker Engine + Compose plugin (Linux)
   - ตรวจสอบ: `docker --version` และ `docker compose version`
2. **Node.js** (มากับ `npx` — ใช้สร้าง password hash)
   - ตรวจสอบ: `node --version`
3. (ถ้าจะเปิดออก Internet) บัญชี **Cloudflare** + โดเมนของตัวเอง

---

## 🚀 ขั้นตอนการติดตั้ง (Step by Step)

### Step 1 — เตรียมโปรเจกต์

```bash
git clone <repo-url> NodeRed-ForAllSystem
cd NodeRed-ForAllSystem
```

### Step 2 — สร้างไฟล์ `.env` จากตัวอย่าง

ไฟล์ `.env` เก็บรหัสผ่าน/โทเคนทั้งหมด และ **ไม่ถูก commit ขึ้น git** (อยู่ใน `.gitignore`)

```bash
# Windows (PowerShell / cmd)
copy .env.example .env

# Mac / Linux
cp .env.example .env
```

### Step 3 — สร้าง Credentials แล้วใส่ลง `.env`

มี 4 ค่าหลักที่ต้องสร้างเอง ทำตาม **[SETUP_CREDENTIALS.md](SETUP_CREDENTIALS.md)** (มีรายละเอียดเต็ม) — สรุปสั้น ๆ:

#### 3.1 Node-RED Password Hash 🔴 จุดพลาดบ่อยที่สุด

สร้าง hash (คำสั่งเดียว พิมพ์รหัสตรง ๆ):

```bash
npx bcryptjs "รหัสผ่านที่ต้องการ" 8
```

จะได้ผลลัพธ์ เช่น:
```
$2b$08$e9VVIc9vJpnsw9LotxfoeOB.EfPAxbZ39HDts2DEbY38Gm5M2r7X2
```

> ### ⚠️⚠️ สำคัญมาก: ต้อง escape `$` เป็น `$$` ก่อนวางลง `.env`
>
> Docker Compose ตีความ `$` เป็นตัวแปร ถ้าวาง hash ดิบ ๆ ลงไป → hash จะขาด → **login ไม่ได้**
>
> | | ค่า |
> |---|---|
> | hash ที่ generator ให้มา | `$2b$08$e9VV...` |
> | ที่ต้องวางใน `.env` (escape แล้ว) | `$$2b$$08$$e9VV...` |
>
> **กฎ:** เปลี่ยน `$` ทุกตัว → `$$`

```env
NODERED_USERNAME=admin
NODERED_PASSWORD_HASH=$$2b$$08$$e9VVIc9vJpnsw9LotxfoeOB.EfPAxbZ39HDts2DEbY38Gm5M2r7X2
```

#### 3.2 N8N Encryption Key (hex 32 ตัว)

```powershell
# Windows PowerShell
[System.Guid]::NewGuid().ToString().Replace("-","").Substring(0,32)
```
```bash
# Mac / Linux
openssl rand -hex 16
```
```env
N8N_ENCRYPTION_KEY=<ค่าที่ได้>
```

#### 3.3 InfluxDB (username / password / org / bucket / token)

```bash
# สร้าง token (hex 64 ตัว) — Mac/Linux
openssl rand -hex 32
```
```powershell
# Windows PowerShell
[System.Guid]::NewGuid().ToString().Replace("-","") + [System.Guid]::NewGuid().ToString().Replace("-","")
```
```env
INFLUXDB_USERNAME=admin
INFLUXDB_PASSWORD=<รหัสผ่านที่แข็งแรง>
INFLUXDB_ORG=iot_org
INFLUXDB_BUCKET=iot_bucket
INFLUXDB_ADMIN_TOKEN=<token ที่สร้าง>
```

#### 3.4 Grafana

```env
GRAFANA_ADMIN_USER=admin
GRAFANA_ADMIN_PASSWORD=<รหัสผ่านที่แข็งแรง>
```

#### 3.5 (ตัวเลือก) Cloudflare Tunnel + N8N Domain

ถ้ายังทดสอบใน local อย่างเดียว **ข้ามได้** (ดู Step 7)
```env
N8N_DOMAIN=n8n.yourdomain.com
CLOUDFLARE_TUNNEL_TOKEN=<token จาก Cloudflare>
```

### Step 4 — เปิดระบบขึ้น

```bash
docker compose up -d
```

Docker จะดึง image และรันทุก service ตามลำดับ (mosquitto และ influxdb ต้อง `healthy` ก่อน Node-RED จึงจะขึ้น)

ดูสถานะ:
```bash
docker compose ps
```
รอจน column STATUS ขึ้น `Up` / `healthy` ทุกตัว

### Step 5 — ตรวจสอบ InfluxDB (init อัตโนมัติจาก `.env`)

> ✅ **ไม่ต้องตั้งค่าเองใน UI แล้ว** — `docker-compose.yml` init InfluxDB ด้วยค่าจาก `.env`
> โดยตรง (`INFLUXDB_ORG` / `INFLUXDB_BUCKET` / `INFLUXDB_ADMIN_TOKEN`) → Node-RED
> และ Grafana เชื่อมได้ทันที

เปิด http://localhost:8086 → login ด้วย `INFLUXDB_USERNAME` / `INFLUXDB_PASSWORD`
แล้วตรวจว่ามี org/bucket ตรงกับ `.env` (ปกติมีให้อัตโนมัติ):

```bash
docker exec influxdb influx org list      # ต้องเห็นค่า INFLUXDB_ORG (เช่น iot_org)
docker exec influxdb influx bucket list    # ต้องเห็นค่า INFLUXDB_BUCKET (เช่น iot_bucket)
```

> ### ⚠️ init ทำงานเฉพาะตอน volume ว่าง (ครั้งแรก) เท่านั้น
>
> ค่า `DOCKER_INFLUXDB_INIT_*` มีผลแค่ตอน InfluxDB ถูกสร้างบน volume เปล่า
> **ถ้าเคยรันไปแล้ว** แต่แก้ org/bucket/token ใน `.env` ทีหลัง ต้องล้าง volume ของ
> InfluxDB ให้ init ใหม่ (ลบเฉพาะ InfluxDB — ข้อมูล service อื่นไม่หาย):
>
> ```bash
> docker compose rm -sf influxdb
> docker volume rm <project>_influxdb_data <project>_influxdb_config
> docker compose up -d influxdb
> ```
> (`<project>` = ชื่อโฟลเดอร์ตัวเล็ก เช่น `nodered-forallsystem` — ดูจาก `docker volume ls`)
> ⚠️ วิธีนี้ **ลบข้อมูลใน InfluxDB** ทั้งหมด ใช้เฉพาะตอนยังไม่มีข้อมูลจริง

### Step 6 — ตั้งค่า service ที่เหลือ

**Node-RED** → http://localhost:1880
- login ด้วย `admin` + รหัส plain ที่ใช้สร้าง hash ใน Step 3.1

**Grafana** → http://localhost:3000
- login ด้วย `GRAFANA_ADMIN_USER` / `GRAFANA_ADMIN_PASSWORD`
- เพิ่ม Data source: **Connections → Add data source → InfluxDB**
  - Query Language: **Flux**
  - URL: `http://influxdb:8086` (ใช้ชื่อ container ไม่ใช่ localhost)
  - Organization: ค่า `INFLUXDB_ORG`
  - Token: ค่า `INFLUXDB_ADMIN_TOKEN`
  - Default Bucket: ค่า `INFLUXDB_BUCKET`

**Home Assistant** → http://localhost:8123
- ครั้งแรกให้สร้างบัญชี owner ตามหน้าจอ

**N8N** → http://localhost:5678
- ครั้งแรกให้สร้างบัญชี owner ตามหน้าจอ

### Step 7 — (ตัวเลือก) เปิดออก Internet ด้วย Cloudflare Tunnel

1. เข้า https://one.dash.cloudflare.com → **Networks → Tunnels → Create a tunnel**
2. เลือกแบบ **Docker** → copy **token** ใส่ `CLOUDFLARE_TUNNEL_TOKEN` ใน `.env`
3. เพิ่ม **Public Hostname**: `n8n.yourdomain.com` → Service: `http://nginx-proxy-manager:80`
4. เปิด **Nginx Proxy Manager** (http://localhost:81, login แรก `admin@example.com` / `changeme` → เปลี่ยนทันที)
   - เพิ่ม **Proxy Host**: Domain `n8n.yourdomain.com` → Forward to `n8n` port `5678`
5. โหลดค่าใหม่:
   ```bash
   docker compose up -d cloudflared nginx-proxy-manager
   ```

---

## 🔧 คำสั่งที่ใช้บ่อย

```bash
# ดูสถานะทุก service
docker compose ps

# ดู log (แบบ realtime) ของ service เดียว
docker compose logs -f nodered

# รีสตาร์ท service เดียว (ไม่โหลดค่า .env ใหม่)
docker compose restart nodered

# โหลดค่า .env ใหม่หลังแก้ไข (ต้องใช้ up -d ไม่ใช่ restart!)
docker compose up -d nodered

# บังคับสร้าง container ใหม่
docker compose up -d --force-recreate nodered

# ปิดทั้งหมด (เก็บข้อมูลไว้)
docker compose down

# ปิดทั้งหมด + ลบข้อมูล (volume) ทั้งหมด — ระวัง!
docker compose down -v

# อัปเดต image เป็นเวอร์ชันล่าสุด
docker compose pull && docker compose up -d
```

> 💡 **จำไว้:** แก้ค่าใน `.env` แล้วต้อง `docker compose up -d <service>` เท่านั้น — `restart` จะยังใช้ค่าเก่า

---

## 🐛 Troubleshooting

### Node-RED login ไม่ได้ (รหัสถูกแต่เข้าไม่ได้)

**สาเหตุอันดับ 1:** ไม่ได้ escape `$` เป็น `$$` ใน `.env`

ตรวจว่า container ได้รับ hash ครบ 60 ตัวหรือไม่:
```bash
docker exec nodered printenv NODERED_PASSWORD_HASH
```
- ✅ ถูก: ได้ hash ยาว **60 ตัว** ขึ้นต้น `$2b$08$` (แสดง `$` เดี่ยว)
- ❌ ผิด: ได้ hash สั้นกว่า 60 → กลับไปแก้ `.env` ให้ `$` ทุกตัวเป็น `$$` แล้ว `docker compose up -d nodered`

**สาเหตุอันดับ 2:** รหัส plain ที่พิมพ์ตอน login ไม่ตรงกับตอนสร้าง hash → สร้าง hash ใหม่

### Node-RED ติดตั้ง custom node เพิ่มไม่ได้ / package.json เพี้ยน

`docker-compose.yml` mount `./nodered/package.json` เข้า container ถ้าฝั่ง host **ไม่มีไฟล์** นี้ Docker จะสร้างเป็น **โฟลเดอร์เปล่า** แทน ทำให้ package.json ใน container พัง

ตรวจสอบ:
```bash
# ฝั่ง host — ต้องเป็นไฟล์ ไม่ใช่ dir
ls -la nodered/package.json
```
ถ้าเป็นโฟลเดอร์ ให้ลบทิ้งแล้วสร้างเป็นไฟล์ (ดูหัวข้อ [ภาคผนวก](#ภาคผนวก-ไฟล์-noderedpackagejson))

### service ไม่ขึ้น / ค้างที่ starting

```bash
docker compose logs <service>     # อ่าน error
docker compose ps                 # ดู health status
```
Node-RED รอ mosquitto + influxdb `healthy` ก่อน ถ้า influxdb ไม่ healthy ให้ดู log ของ influxdb ก่อน

### InfluxDB / Grafana เชื่อมกันไม่ได้

- ใน Grafana ต้องใช้ URL `http://influxdb:8086` (ชื่อ container) **ไม่ใช่** `localhost`
- org / bucket / token ต้องตรงกับที่สร้างใน InfluxDB UI (ดู Step 5)

---

## 📁 โครงสร้างโปรเจกต์

```
NodeRed-ForAllSystem/
├── docker-compose.yml          # นิยาม service ทั้งหมด
├── .env                        # secrets (ไม่ commit)
├── .env.example                # template ของ .env
├── README.md                   # ← ไฟล์นี้
├── SETUP_CREDENTIALS.md        # วิธีสร้าง hash / key / token
├── mosquitto/
│   └── config/mosquitto.conf   # ตั้งค่า MQTT broker
├── nodered/
│   ├── settings.js             # ตั้งค่า Node-RED (อ่าน env credentials)
│   └── package.json            # custom nodes (ต้องเป็นไฟล์ ดูภาคผนวก)
├── flows/                      # ตัวอย่าง flow Node-RED / workflow n8n
└── esp32-firmware-NodeRED/     # firmware ฝั่งอุปกรณ์ (PlatformIO)
```

---

## ภาคผนวก: ไฟล์ `nodered/package.json`

ควรเป็น **ไฟล์** หน้าตาแบบนี้ (ไม่ใช่โฟลเดอร์) เพื่อให้ Node-RED จัดการ custom nodes ได้:

```json
{
  "name": "node-red-project",
  "description": "Node-RED custom nodes",
  "version": "1.0.0",
  "dependencies": {
    "node-red-contrib-influxdb": "*"
  }
}
```

---

## 🔗 เอกสารที่เกี่ยวข้อง

- **[DEPLOY_VPS.md](DEPLOY_VPS.md)** — 🚀 deploy ขึ้น VPS จริง (production) แบบ step-by-step + security hardening
- **[SETUP_CREDENTIALS.md](SETUP_CREDENTIALS.md)** — สร้าง password hash, encryption key, token แบบละเอียด
- **[esp32-firmware-NodeRED/README.md](esp32-firmware-NodeRED/README.md)** — firmware ฝั่ง ESP32
